#include "usvfs_fuse/usvfsmanager.h"

#include "usvfs_fuse/fdmap.h"
#include "usvfs_fuse/logger.h"
#include "usvfs_fuse/mountstate.h"
#include "usvfs_fuse/usvfs.h"
#include "usvfs_fuse/usvfs_version.h"
#include "usvfs_fuse/utils.h"
#include "usvfs_fuse/virtualfiletreeitem.h"

using namespace std;
using namespace Qt::StringLiterals;
namespace fs = std::filesystem;

namespace
{

shared_ptr<VirtualFileTreeItem> createFileTree(const string& path, FdMap& fdMap)
{
  logger::debug("creating file tree for {}", path);
  error_code ec;
  auto fileTree = make_shared<VirtualFileTreeItem>("/", path, dir);

  int fd = open(path.c_str(), OPEN_FLAGS, OPEN_PERMS);
  if (fd == -1) {
    throw runtime_error(
        format("error opening directory {}: {}", path, strerror(errno)));
  }
  logger::trace("adding fd {} for {}", fd, path);
  fdMap[path] = fd;

  fs::recursive_directory_iterator iter(path, ec);
  if (ec) {
    throw runtime_error("error creating file tree: "s + ec.message());
  }
  for (const fs::directory_entry& entry : iter) {
    fs::path relative = fs::relative(entry.path(), path);

    logger::debug("adding '{}' to file tree", relative.string());
    auto newItem =
        fileTree->add(relative.string(), entry.path().string(),
                      entry.status().type() == fs::file_type::directory ? dir : file);
    if (newItem == nullptr) {
      throw runtime_error("error adding "s + relative.string() + " to file tree");
    }

    if (entry.is_directory()) {
      fd = open(entry.path().string().c_str(), OPEN_FLAGS, OPEN_PERMS);
      if (fd == -1) {
        throw runtime_error(
            format("error opening directory {}: {}", path, strerror(errno)));
      }
      logger::trace("adding fd {} for {}", fd, entry.path().string());
      fdMap[entry.path().string()] = fd;
    }
  }
  return fileTree;
}

fuse_operations createOperations()
{
  fuse_operations ops = {};
  ops.getattr         = usvfs_getattr;
  ops.readlink        = usvfs_readlink;
  // ops.mknod
  ops.mkdir    = usvfs_mkdir;
  ops.unlink   = usvfs_unlink;
  ops.rmdir    = usvfs_rmdir;
  ops.symlink  = usvfs_symlink;
  ops.rename   = usvfs_rename;
  ops.link     = usvfs_link;
  ops.chmod    = usvfs_chmod;
  ops.chown    = usvfs_chown;
  ops.truncate = usvfs_truncate;
  ops.open     = usvfs_open;
  ops.read     = usvfs_read;
  ops.write    = usvfs_write;
  ops.statfs   = usvfs_statfs;
  ops.flush    = usvfs_flush;
  ops.release  = usvfs_release;
  ops.fsync    = usvfs_fsync;
  // setxattr
  // getxattr
  // listxattr
  // listxattr
  // removexattr
  // ops.opendir = usvfs_opendir;
  ops.readdir    = usvfs_readdir;
  ops.releasedir = usvfs_releasedir;
  ops.fsyncdir   = usvfs_fsyncdir;
  // init
  // destroy
  // access
  ops.create = usvfs_create;
  // lock
  // utimens
  ops.bmap = nullptr;
  // ioctl
  // poll
  // write_buf
  // read_buf
  // flock
  // fallocate
  // copy_file_range
  // lseek
  return ops;
}

}  // namespace

UsvfsManager::~UsvfsManager() noexcept
{
  unmount();
}

void UsvfsManager::usvfsClearVirtualMappings() noexcept
{
  unique_lock lock(m_mtx);
  m_pendingMounts.clear();
}

bool UsvfsManager::usvfsVirtualLinkFile(const char* source, const char* destination,
                                        unsigned int flags) noexcept
{
  unique_lock lock(m_mtx);

  const fs::path srcPath = fs::path(source);
  const fs::path dstPath = fs::path(destination);

  FdMap fdMap;

  string dstDir = dstPath.parent_path().string();

  if (fileNameInSkipSuffixes(srcPath.filename().string())) {
    return flags & linkFlag::FAIL_IF_SKIPPED ? false : true;
  }

  if (flags & linkFlag::FAIL_IF_EXISTS) {
    if (pathExists(destination)) {
      errno = EEXIST;
      return false;
    }
  }

  // check if destination exists in pending mounts
  for (const auto& state : m_pendingMounts) {
    if (state->mountpoint == dstDir) {
      // destination exists, add to the existing file tree
      auto result = state->fileTree->add(dstPath.filename().string(), source, file);
      if (result != nullptr) {
        string parentDir = getParentPath(source);
        int fd           = open(parentDir.c_str(), OPEN_FLAGS, OPEN_PERMS);
        if (fd == -1) {
          logger::error("open() failed for {}: {}", parentDir, strerror(errno));
          return false;
        }
        logger::trace("adding fd {} for {}", fd, parentDir);
        state->fdMap[parentDir] = fd;
      }
      return result != nullptr;
    }
  }

  VirtualFileTreeItem sourceFileTree("/", dstDir, dir);
  auto result = sourceFileTree.add(dstPath.filename().string(), source, file, true);
  if (result != nullptr) {
    return false;
  }
  {
    string parentDir = getParentPath(source);
    int fd           = open(parentDir.c_str(), OPEN_FLAGS, OPEN_PERMS);
    if (fd == -1) {
      logger::error("open() failed for {}: {}", parentDir, strerror(errno));
      return false;
    }
    logger::trace("adding fd {} for {}", fd, parentDir);
    fdMap[parentDir] = fd;
  }

  // create the file tree for existing files
  shared_ptr<VirtualFileTreeItem> destinationFileTree =
      createFileTree(destination, fdMap);

  *destinationFileTree += sourceFileTree;

  // prepare state and enqueue to the pending list (no mounting yet)
  auto state        = make_unique<MountState>();
  state->fileTree   = destinationFileTree;
  state->mountpoint = dstDir;
  state->fdMap      = fdMap;
  m_pendingMounts.emplace_back(std::move(state));

  return true;
}

bool UsvfsManager::usvfsVirtualLinkDirectoryStatic(const char* source,
                                                   const char* destination,
                                                   unsigned int flags) noexcept
{
  unique_lock lock(m_mtx);
  // TODO: make check case insensitive?
  if (flags & linkFlag::FAIL_IF_EXISTS && pathExists(destination)) {
    errno = EEXIST;
    return false;
  }

  error_code ec;
  FdMap fdMap;
  {
    int fd = open(source, OPEN_FLAGS, OPEN_PERMS);
    if (fd == -1) {
      logger::error("error opening {}: {}", source, strerror(errno));
      return false;
    }
    logger::trace("adding fd {} for {}", fd, source);
    fdMap[source] = fd;
  }

  VirtualFileTreeItem sourceFileTree("/", source, dir);
  if (flags & linkFlag::RECURSIVE) {
    // create the file tree
    fs::recursive_directory_iterator iter(source, ec);
    if (ec) {
      logger::error("error creating recursive_directory_iterator: {}", ec.message());
      return false;
    }
    for (const fs::directory_entry& entry : iter) {
      // check if the directory should be skipped
      string fileName = entry.path().filename().string();
      if ((entry.is_directory() && fileNameInSkipDirectories(fileName)) ||
          (!entry.is_directory() && fileNameInSkipSuffixes(fileName))) {
        // Fail if we desire to fail when a dir/file is skipped
        if (flags & linkFlag::FAIL_IF_SKIPPED) {
          logger::debug("{} '{}' skipped, failing as defined by link flags",
                        entry.is_directory() ? "directory" : "file", fileName);
          return false;
        }

        continue;
      }

      fs::path relative = fs::relative(entry.path(), source);

      logger::debug("adding '{}' to file tree", relative.string());
      auto newItem = sourceFileTree.add(
          relative.string(), entry.path().string(),
          entry.status().type() == filesystem::file_type::directory ? dir : file);
      if (newItem == nullptr) {
        logger::error("error adding '{}' to file tree", relative.string());
        return false;
      }
      if (entry.is_directory()) {
        int fd = open(entry.path().string().c_str(), OPEN_FLAGS, OPEN_PERMS);
        if (fd == -1) {
          logger::error("open('{}') failed: {}", entry.path().string(),
                        strerror(errno));
          return false;
        }
        fdMap[entry.path().string()] = fd;
        logger::trace("adding fd {} for {}, real path: {}", fd, fileName,
                      newItem->realPath());
      }
    }
  } else {
    // TODO: check what upstream usvfs really does in this case
  }

  // check if destination exists in pending mounts
  for (const auto& state : m_pendingMounts) {
    if (state->mountpoint == destination) {
      // destination exists, merge file trees
      *state->fileTree += sourceFileTree;
      for (const auto& [path, fd] : fdMap) {
        state->fdMap[path] = fd;
      }
      return true;
    }
  }

  // create the file tree for existing files
  shared_ptr<VirtualFileTreeItem> destinationFileTree =
      createFileTree(destination, fdMap);

  *destinationFileTree += sourceFileTree;

  // prepare state and add to the pending list (no mounting yet)
  auto state        = make_unique<MountState>();
  state->fileTree   = destinationFileTree;
  state->mountpoint = destination;
  state->fdMap      = fdMap;

  m_pendingMounts.emplace_back(std::move(state));

  return true;
}

std::vector<std::unique_ptr<QProcess>>& UsvfsManager::usvfsGetVFSProcessList() noexcept
{
  shared_lock lock(m_mtx);
  return m_spawnedProcesses;
}

bool UsvfsManager::usvfsCreateProcessHooked(const char* file, const char* arg,
                                            const char* workDir, char** envp) noexcept
{
  scoped_lock lock(m_mtx);
  logger::trace("{}: {}, {}, {}", __FUNCTION__, file, arg, workDir);

  bool blacklisted = m_executableBlacklist.contains(file);

  if (!blacklisted) {
    try {
      mount();
    } catch (const std::exception& e) {
      logger::error("mount failed: {}", e.what());
      return false;
    }
  }

  auto p = make_unique<QProcess>();

  const QString program  = QString::fromLocal8Bit(file);
  const QStringList args = QProcess::splitCommand(QString::fromLocal8Bit(arg));

  QStringList env;
  for (int i = 0; envp[i] != nullptr; ++i) {
    env << envp[i];
  }

  const bool wine =
      program.endsWith("wine"_L1) || program.endsWith("wine-staging"_L1) ||
      program.endsWith("wine64"_L1) || program.endsWith("wine64-staging"_L1);
  const bool proton = program.endsWith("proton"_L1);

  if (wine || proton) {
    if (!m_forceLoadLibraries.empty()) {
      const QString processName = wine ? args.at(0) : args.at(1);
      const std::vector<std::string> applicableLibraries =
          librariesToForceLoad(processName.toStdString());
      if (!applicableLibraries.empty()) {
        QString dllOverrides = "WINEDLLOVERRIDES=\"";
        for (size_t i = 0; i < applicableLibraries.size() - 1; ++i) {
          dllOverrides += applicableLibraries[i] + "=n,b;";
        }
        dllOverrides += applicableLibraries.back() + "=n,b\"";
        env << dllOverrides;
        logger::debug("adding '{}' to process", dllOverrides.toStdString());
      }
    }
  }

  p->setEnvironment(env);
  p->setWorkingDirectory(workDir);
  p->setProgram(program);
  p->setArguments(args);

  // automatically unmount if no processes are left
  QObject::connect(p.get(), &QProcess::finished, [&](int exitCode) {
    logger::info("process '{}' has exited with code {}", file, exitCode);
    if (!anyProcessRunning()) {
      logger::info("last process has exited, unmounting");
      unmount();
    }
  });

  this_thread::sleep_for(m_processDelay);

  p->start();

  m_spawnedProcesses.emplace_back(std::move(p));

  return true;
}

bool UsvfsManager::usvfsCreateProcessHooked(const char* file, const char* arg,
                                            const char* workDir) noexcept
{
  return usvfsCreateProcessHooked(file, arg, workDir, environ);
}

bool UsvfsManager::usvfsCreateProcessHooked(const char* file, const char* arg) noexcept
{
  char* cwd            = get_current_dir_name();
  const string workDir = cwd;
  free(cwd);
  return usvfsCreateProcessHooked(file, arg, workDir.c_str(), environ);
}

std::string UsvfsManager::usvfsCreateVFSDump() const noexcept
{
  shared_lock lock(m_mtx);
  ostringstream oss;
  string result;
  logger::debug("dumping {} pending and {} active mounts", m_pendingMounts.size(),
                m_mounts.size());
  for (const auto& state : m_pendingMounts) {
    state->fileTree->dumpTree(oss);
  }

  for (const auto& state : m_mounts) {
    state->fileTree->dumpTree(oss);
  }

  return oss.str();
}

void UsvfsManager::usvfsBlacklistExecutable(const std::string& executableName) noexcept
{
  scoped_lock lock(m_mtx);
  logger::debug("blacklisting '{}'", executableName);
  m_executableBlacklist.emplace(executableName);
}

void UsvfsManager::usvfsClearExecutableBlacklist() noexcept
{
  scoped_lock lock(m_mtx);
  logger::debug("clearing blacklist");
  m_executableBlacklist.clear();
}

void UsvfsManager::usvfsAddSkipFileSuffix(const std::string& fileSuffix) noexcept
{
  if (fileSuffix.empty()) {
    return;
  }

  scoped_lock lock(m_mtx);
  logger::debug("added skip file suffix '{}'", fileSuffix);
  m_skipFileSuffixes.emplace(fileSuffix);
}

void UsvfsManager::usvfsClearSkipFileSuffixes() noexcept
{
  scoped_lock lock(m_mtx);
  logger::debug("clearing skip file suffixes");
  m_skipFileSuffixes.clear();
}

void UsvfsManager::usvfsAddSkipDirectory(const std::string& directory) noexcept
{
  if (directory.empty()) {
    return;
  }

  scoped_lock lock(m_mtx);
  logger::debug("added skip directory '{}'", directory);
  m_skipDirectories.emplace(directory);
}

void UsvfsManager::usvfsClearSkipDirectories()
{
  scoped_lock lock(m_mtx);
  logger::debug("clearing skip directories");
  m_skipDirectories.clear();
}

void UsvfsManager::usvfsForceLoadLibrary(const std::string& processName,
                                         const std::string& libraryPath) noexcept
{
  scoped_lock lock(m_mtx);
  logger::debug("adding forced library '{}' for process '{}'", libraryPath,
                processName);
  m_forceLoadLibraries.push_back({processName, libraryPath});
}

void UsvfsManager::usvfsClearLibraryForceLoads() noexcept
{
  scoped_lock lock(m_mtx);
  logger::debug("clearing forced libraries");
  m_forceLoadLibraries.clear();
}

void UsvfsManager::usvfsPrintDebugInfo() noexcept
{
#warning STUB
  // std::string shmName = "SHM_NAME";
  // logger::warn("===== debug {} =====",
  //                            shmName);
  // void* buffer      = nullptr;
  // size_t bufferSize = 0;
  // // context->redirectionTable().getBuffer(buffer, bufferSize);
  //
  // std::ostringstream temp;
  // for (size_t i = 0; i < bufferSize; ++i) {
  //   temp << std::hex << std::setfill('0') << std::setw(2)
  //        << (unsigned)reinterpret_cast<char*>(buffer)[i] << " ";
  //   if ((i % 16) == 15) {
  //     logger::info("{}", temp.str());
  //     temp.str("");
  //     temp.clear();
  //   }
  // }
  // if (!temp.str().empty()) {
  //   logger::info("{}", temp.str());
  // }
  // logger::warn("===== / debug {} =====",
  //                            shmName);
}

void UsvfsManager::enableDebugMode() noexcept
{
  m_debugMode = true;
}

void UsvfsManager::setProcessDelay(std::chrono::milliseconds processDelay) noexcept
{
  m_processDelay = processDelay;
}

std::shared_ptr<spdlog::logger> UsvfsManager::setupLogger(
    std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks) noexcept
{
  static constexpr std::string loggerName = "usvfs";

  // a temporary logger was created in ctor
  spdlog::drop(loggerName);

  auto logger = spdlog::get(loggerName);
  if (logger == nullptr) {
    if (!sinks.empty()) {
      logger = std::make_shared<spdlog::logger>(loggerName, std::begin(sinks),
                                                std::end(sinks));
      spdlog::register_logger(logger);
    } else {
      logger = spdlog::create<spdlog::sinks::stdout_sink_mt>(loggerName);
    }
    logger->set_pattern("%H:%M:%S.%e [%L] %v");
    logger->set_level(spdlog::level::debug);
  } else {
    logger::warn("logger already existed");
  }

  logger::info("usvfs library {} initialized in process {}", USVFS_VERSION_STRING,
               getpid());

  return logger;
}

const char* UsvfsManager::usvfsVersionString()
{
  return USVFS_VERSION_STRING;
}

void UsvfsManager::mount() noexcept(false)
{
  logger::info("mounting {} mount points", m_pendingMounts.size());
  scoped_lock lock(m_mtx);

  // move pending to a local list
  if (m_pendingMounts.empty()) {
    return;
  }
  vector<unique_ptr<MountState>> toMount;
  toMount.swap(m_pendingMounts);

  int fd;
  if (!m_upperDir.empty()) {
    fd = open(m_upperDir.c_str(), OPEN_FLAGS, OPEN_PERMS);
    if (fd == -1) {
      throw runtime_error("error opening upperDir: "s + strerror(errno));
    }
  }

  // start a thread for each pending mount
  for (auto& state : toMount) {
    if (!m_upperDir.empty()) {
      state->upperDir = m_upperDir;
      logger::trace("adding fd {} for {}", fd, m_upperDir);
      state->fdMap[m_upperDir] = fd;
    }
    try {
      thread t([s = std::move(state), this]() mutable {
        run_fuse(std::move(s));
      });
      t.detach();
    } catch (const exception& e) {
      // the remaining entries are dropped since toMount owns them and will be destroyed
      throw runtime_error("Failed to create FUSE thread: "s + e.what());
    }
  }
  // this_thread::sleep_for(100ms);
}

bool UsvfsManager::unmount() noexcept
{
  scoped_lock lock(m_mtx);

  logger::debug("unmounting {} mounts", m_mounts.size());

  if (anyProcessRunning()) {
    logger::warn("there is still at least one process running, not unmounting");
    return false;
  }

  for (std::unique_ptr<MountState>& mount : m_mounts) {
    logger::debug("unmounting {}", mount->mountpoint);
    fuse_unmount(mount->fusePtr);
    fuse_destroy(mount->fusePtr);
  }
  m_mounts.clear();

  return true;
}

void UsvfsManager::setUpperDir(std::string upperDir) noexcept
{
  m_upperDir = std::move(upperDir);
}

UsvfsManager::UsvfsManager() noexcept
{
  umask(0);

  auto logger = spdlog::get("usvfs");
  if (logger == nullptr) {
    logger = spdlog::create<spdlog::sinks::stdout_sink_mt>("usvfs");
    logger->set_pattern("%H:%M:%S.%e [%L] %v");
    logger->set_level(spdlog::level::info);
  }
}

void UsvfsManager::run_fuse(std::unique_ptr<MountState> state)
{
  const char* opts = m_debugMode ? "default_permissions,debug" : "default_permissions";
  const char* argv[] = {"usvfs_fuse", "-o", opts};
  int argc           = 3;
  fuse_args args     = FUSE_ARGS_INIT(argc, const_cast<char**>(argv));

  fuse_operations ops = createOperations();

  MountState* raw = state.get();
  raw->fusePtr    = fuse_new(&args, &ops, sizeof(fuse_operations), raw);
  fuse_opt_free_args(&args);
  if (!raw->fusePtr) {
    // Couldn't create FUSE handle; drop the mount
    return;
  }
  if (fuse_mount(raw->fusePtr, raw->mountpoint.c_str()) != 0) {
    fuse_destroy(raw->fusePtr);
    raw->fusePtr = nullptr;
    return;
  }

  {
    scoped_lock lock(m_mtx);
    m_mounts.emplace_back(std::move(state));
  }

  // Enter loop; this blocks until unmounted
  fuse_loop(raw->fusePtr);
}

bool UsvfsManager::fileNameInSkipSuffixes(const std::string& fileName) const noexcept
{
  return fileNameInSkipSuffixes(fileName, m_skipFileSuffixes);
}

bool UsvfsManager::fileNameInSkipSuffixes(
    const std::string& fileName, const std::set<std::string>& skipSuffixes) noexcept
{
  return ranges::any_of(skipSuffixes, [&](const std::string& suffix) {
    if (iendsWith(fileName, suffix)) {
      logger::debug("file '{}' should be skipped, matches file suffix '{}'", fileName,
                    suffix);
      return true;
    }
    return false;
  });
}

bool UsvfsManager::fileNameInSkipDirectories(
    const std::string& directoryName) const noexcept
{
  return fileNameInSkipDirectories(directoryName, m_skipDirectories);
}

bool UsvfsManager::fileNameInSkipDirectories(
    const std::string& directoryName,
    const std::set<std::string>& skipDirectories) noexcept
{
  return ranges::any_of(skipDirectories, [&](const std::string& suffix) {
    if (iendsWith(directoryName, suffix)) {
      logger::debug("directory '{}' should be skipped", directoryName);
      return true;
    }
    return false;
  });
}

bool UsvfsManager::pathExists(const std::string& path) const noexcept
{
  if (fs::exists(path)) {
    return true;
  }

  for (const auto& mount : m_pendingMounts) {
    if (mount->fileTree->find(path) != nullptr) {
      return true;
    }
  }

  return false;
}

std::vector<std::string>
UsvfsManager::librariesToForceLoad(const std::string& processName) const noexcept
{
  vector<string> result;

  for (const auto& library : m_forceLoadLibraries) {
    if (iequals(library.processName, processName)) {
      result.emplace_back(library.libraryPath);
    }
  }

  return result;
}

bool UsvfsManager::anyProcessRunning() const noexcept
{
  return ranges::any_of(m_spawnedProcesses, [&](const unique_ptr<QProcess>& process) {
    return process->state() == QProcess::Running;
  });
}
