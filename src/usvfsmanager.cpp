#include "usvfs-fuse/usvfsmanager.h"

#include "loghelpers.h"
#include "usvfs-fuse/fdmap.h"
#include "usvfs-fuse/logger.h"
#include "usvfs-fuse/mountstate.h"
#include "usvfs-fuse/usvfs.h"
#include "usvfs-fuse/usvfs_version.h"
#include "usvfs-fuse/utils.h"
#include "usvfs-fuse/virtualfiletreeitem.h"

using namespace std;
namespace fs = std::filesystem;

namespace
{

constexpr int pollTimeout        = 10;
constexpr size_t stackSize       = 1024 * 1024;       // stack size for cloned child
constexpr size_t maxLogFileSize  = 1024 * 1024 * 10;  // 10 MiB
constexpr size_t maxLogFileCount = 10;

shared_ptr<VirtualFileTreeItem> createFileTree(const string& path, FdMap& fdMap)
{
  logger::debug("creating file tree for {}", path);
  error_code ec;
  auto fileTree = make_shared<VirtualFileTreeItem>("/", path, dir);

  int fd = open(path.c_str(), OPEN_FLAGS);
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
      fd = open(entry.path().string().c_str(), OPEN_FLAGS);
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

void writeToFile(const string& filename, string_view content)
{
  ofstream ofs(filename);
  ofs.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  ofs << content;
}

int childFunc(void* arg)
{
  auto* state = static_cast<MountState*>(arg);

  // remap uid
  writeToFile("/proc/self/uid_map", format("0 {} 1", state->uid));
  // deny setgroups (see user_namespaces(7))
  writeToFile("/proc/self/setgroups", "deny");
  // remap gid
  writeToFile("/proc/self/gid_map", format("0 {} 1", state->gid));

  // enter existing namespace
  if (state->nsFd != -1) {
    logger::debug("usvfs entering existing namespace");
    int result = setns(state->nsFd, CLONE_NEWUSER | CLONE_NEWNS);
    if (result == -1) {
      logger::error("setns() failed: {}", strerror(errno));
      return -1;
    }
  }

  const char* argv[] = {"usvfs_fuse", "-o", "default_permissions"};
  int argc           = 3;
  fuse_args args     = FUSE_ARGS_INIT(argc, const_cast<char**>(argv));

  fuse_operations ops = createOperations();

  state->fusePtr = fuse_new(&args, &ops, sizeof(fuse_operations), state);
  fuse_opt_free_args(&args);
  if (state->fusePtr == nullptr) {
    // Couldn't create FUSE handle; drop the mount
    logger::error("fuse_new() failed");
    return -1;
  }
  if (fuse_mount(state->fusePtr, state->mountpoint.c_str()) == -1) {
    fuse_destroy(state->fusePtr);
    state->fusePtr = nullptr;
    logger::error("fuse_mount() failed for mountpoint {}: {}", state->mountpoint,
                  strerror(errno));
    return -1;
  }

  // set signal handlers
  fuse_session* session = fuse_get_session(state->fusePtr);
  fuse_set_signal_handlers(session);

  // enter loop; this blocks until unmounted or interrupted by the signal handler
  fuse_loop(state->fusePtr);

  fuse_unmount(state->fusePtr);
  fuse_destroy(state->fusePtr);
  state->fusePtr = nullptr;

  return 0;
}

vector<string> createEnv()
{
  vector<string> env;
  for (int i = 0; environ[i] != nullptr; ++i) {
    env.emplace_back(environ[i]);
  }
  return env;
}

}  // namespace

UsvfsManager::~UsvfsManager() noexcept
{
  unmount();
}

void UsvfsManager::usvfsClearVirtualMappings() noexcept
{
  scoped_lock lock(m_mtx);
  m_pendingMounts.clear();
}

bool UsvfsManager::usvfsVirtualLinkFile(const std::string& source,
                                        const std::string& destination,
                                        unsigned int flags) noexcept
{
  scoped_lock lock(m_mtx);

  logger::trace("{}, source: {}, destination: {}", __FUNCTION__, source, destination);

  const fs::path srcPath = fs::path(source);
  const fs::path dstPath = fs::path(destination);

  FdMap fdMap;

  string dstDir = dstPath.parent_path().string();

  if (fileNameInSkipSuffixes(srcPath.filename().string())) {
    logger::debug("file {} should be skipped", source);
    return flags & linkFlag::FAIL_IF_SKIPPED ? false : true;
  }

  if (flags & linkFlag::FAIL_IF_EXISTS) {
    if (pathExists(destination)) {
      logger::debug("destination {} exists, not linking", destination);
      errno = EEXIST;
      return false;
    }
  }

  // check if destination exists in pending mounts
  for (const auto& state : m_pendingMounts) {
    if (state->mountpoint == dstDir) {
      logger::debug("mountpoint already exists, adding to file tree");
      // destination exists, add to the existing file tree
      auto result = state->fileTree->add(dstPath.filename().string(), source, file);
      if (result != nullptr) {
        string parentDir = getParentPath(source);
        int fd           = open(parentDir.c_str(), OPEN_FLAGS);
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

  string srcParentDir = getParentPath(source);
  string dstParentDir = getParentPath(destination);

  // open a file descriptor for the source parent directory
  int fd = open(srcParentDir.c_str(), OPEN_FLAGS);
  if (fd == -1) {
    logger::error("open() failed for {}: {}", srcParentDir, strerror(errno));
    return false;
  }
  logger::trace("adding fd {} for {}", fd, srcParentDir);
  fdMap[srcParentDir] = fd;

  // open a file descriptor for the destination parent directory
  fd = open(dstParentDir.c_str(), OPEN_FLAGS);
  if (fd == -1) {
    logger::error("open() failed for {}: {}", dstParentDir, strerror(errno));
    return false;
  }
  logger::trace("adding fd {} for {}", fd, dstParentDir);
  fdMap[dstParentDir] = fd;

  // create the file tree for existing files
  shared_ptr<VirtualFileTreeItem> destinationFileTree =
      createFileTree(dstParentDir, fdMap);

  auto result =
      destinationFileTree->add(dstPath.filename().string(), source, file, true);
  if (result == nullptr) {
    return false;
  }

  // prepare state and enqueue to the pending list (no mounting yet)
  auto state        = make_unique<MountState>();
  state->fileTree   = destinationFileTree;
  state->mountpoint = dstDir;
  state->fdMap      = fdMap;
  m_pendingMounts.emplace_back(std::move(state));

  return true;
}

bool UsvfsManager::usvfsVirtualLinkDirectoryStatic(const std::string& source,
                                                   const std::string& destination,
                                                   unsigned int flags) noexcept
{
  scoped_lock lock(m_mtx);

  logger::trace("{}, source: {}, destination: {}", __FUNCTION__, source, destination);

  // TODO: make check case insensitive?
  if (flags & linkFlag::FAIL_IF_EXISTS && pathExists(destination)) {
    errno = EEXIST;
    return false;
  }

  error_code ec;
  FdMap fdMap;
  {
    int fd = open(source.c_str(), OPEN_FLAGS);
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
        int fd = open(entry.path().string().c_str(), OPEN_FLAGS);
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

const std::vector<pid_t>& UsvfsManager::usvfsGetVFSProcessList() const noexcept
{
  shared_lock lock(m_mtx);
  return m_spawnedProcesses;
}

pid_t UsvfsManager::usvfsCreateProcessHooked(const std::string& file,
                                             const std::string& arg,
                                             const std::string& workDir,
                                             std::vector<std::string> env) noexcept
{
  scoped_lock lock(m_mtx);

  // sanity check
  if (!m_mounts.empty() && m_nsPidFd == -1) {
    logger::error("usvfs is mounted without any reference to a namespace, aborting");
    return false;
  }

  logger::trace("{}: {}, {}, {}", __FUNCTION__, file, arg, workDir);

  if (!m_executableBlacklist.contains(file)) {
    if (!mountInternal()) {
      return -1;
    }
  }

  // handle wine dll overrides
  const bool wine = file.ends_with("wine") || file.ends_with("wine-staging") ||
                    file.ends_with("wine64") || file.ends_with("wine64-staging");
  const bool proton = file.ends_with("proton");

  if (wine || proton) {
    if (!m_forceLoadLibraries.empty()) {
      const size_t firstSpace = arg.find_first_of(' ');
      const string processName =
          wine ? arg.substr(0, firstSpace - 1)
               : arg.substr(firstSpace, arg.find_first_of(' ') - 1);
      logger::trace("using process name {}", processName);
      const vector<string> applicableLibraries = librariesToForceLoad(processName);
      if (!applicableLibraries.empty()) {
        string dllOverrides = "WINEDLLOVERRIDES=\"";
        for (size_t i = 0; i < applicableLibraries.size() - 1; ++i) {
          dllOverrides += applicableLibraries[i] + "=n,b;";
        }
        dllOverrides += applicableLibraries.back() + "=n,b\"";
        env.emplace_back(dllOverrides);
        logger::debug("adding '{}' to process", dllOverrides);
      }
    }
  }

  const string cmd = "'" + file + "' " + arg;
  logger::debug("{}: command string: {}", __FUNCTION__, cmd);

  int pipefd[2];

  int result = pipe(pipefd);
  if (result == -1) {
    logger::error("pipe failed: {}", strerror(errno));
    return -1;
  }

  const pid_t pid = fork();

  // error
  if (pid == -1) {
    close(pipefd[0]);
    close(pipefd[1]);

    logger::error("fork failed: {}", strerror(errno));
    return -1;
  }

  // child
  if (pid == 0) {
    // close read end
    close(pipefd[0]);
    // set CLOEXEC on write end
    fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);

    if (m_useMountNamespace) {
      if (setns(m_nsPidFd, CLONE_NEWUSER | CLONE_NEWNS) == -1) {
        logger::error("setns failed: {}", strerror(errno));
        exit(EXIT_FAILURE);
      }
    }

    if (chdir(workDir.c_str()) == -1) {
      logger::error("chdir failed: {}", strerror(errno));
    }

    // create environment
    char** envp = static_cast<char**>(malloc((env.size() + 1) * sizeof(char*)));
    int i       = 0;
    for (const auto& v : env) {
      envp[i++] = strdup(v.c_str());
    }
    envp[i] = nullptr;

    execle("/bin/sh", "/bin/sh", "-c", cmd.c_str(), nullptr, envp);

    // write error to pipe
    const int error = errno;
    if (write(pipefd[1], &error, sizeof(int)) == -1) {
      logger::error("Error writing exec error to pipe: {}\n Exec error was {}",
                    strerror(errno), strerror(error));
    }

    _exit(EXIT_FAILURE);
  }

  // parent

  // close write end
  close(pipefd[1]);

  int error;
  size_t count = read(pipefd[0], &error, sizeof(int));

  // close read end
  close(pipefd[0]);

  // check result
  if (count == 0) {
    // success
    return pid;
  }

  logger::error("execl failed: {}", strerror(error));
  return -1;
}

pid_t UsvfsManager::usvfsCreateProcessHooked(const std::string& file,
                                             const std::string& arg,
                                             const std::string& workDir) noexcept
{
  return usvfsCreateProcessHooked(file, arg, workDir, createEnv());
}

pid_t UsvfsManager::usvfsCreateProcessHooked(const std::string& file,
                                             const std::string& arg) noexcept
{
  char* cwd            = get_current_dir_name();
  const string workDir = cwd;
  free(cwd);
  return usvfsCreateProcessHooked(file, arg, workDir, createEnv());
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

void UsvfsManager::usvfsClearSkipDirectories() noexcept
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

void UsvfsManager::setDebugMode(bool value) noexcept
{
  scoped_lock lock(m_mtx);
  m_debugMode = value;
}

void UsvfsManager::setProcessDelay(std::chrono::milliseconds processDelay) noexcept
{
  scoped_lock lock(m_mtx);
  m_processDelay = processDelay;
}

void UsvfsManager::setLogLevel(LogLevel logLevel) noexcept
{
  scoped_lock lock(m_mtx);
  spdlog::get("usvfs")->set_level(ConvertLogLevel(logLevel));
}

void UsvfsManager::setLogFile(const std::string& logFile) noexcept
{
  scoped_lock lock(m_mtx);

  if (m_fileSink == nullptr) {
    shared_ptr<spdlog::logger> logger = spdlog::get("usvfs");
    vector<spdlog::sink_ptr> sinks    = logger->sinks();

    m_fileSink = make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logFile, maxLogFileSize, maxLogFileCount, true);
    m_fileSink->set_level(spdlog::level::debug);
    sinks.emplace_back(m_fileSink);

    logger = make_shared<spdlog::logger>("usvfs", sinks.begin(), sinks.end());

    spdlog::register_or_replace(logger);
  }
}

const char* UsvfsManager::usvfsVersionString() noexcept
{
  return USVFS_VERSION_STRING;
}

bool UsvfsManager::mount() noexcept
{
  scoped_lock lock(m_mtx);
  return mountInternal();
}

bool UsvfsManager::unmount() noexcept
{
  scoped_lock lock(m_mtx);
  if (m_mounts.empty()) {
    return true;
  }

  logger::info("unmounting {} mounts", m_mounts.size());

  if (anyProcessRunning()) {
    logger::warn("there is still at least one process running, not unmounting");
    return false;
  }

  for (std::unique_ptr<MountState>& mount : m_mounts) {
    logger::debug("unmounting {}", mount->mountpoint);
    if (m_useMountNamespace) {
      if (mount->pidFd == -1) {
        logger::warn("mount pidFd is -1");
        return false;
      }

      siginfo_t info;
      if (pidfd_send_signal(mount->pidFd, SIGINT, nullptr, 0) == -1) {
        logger::error("pidfd_send_signal() failed: {}", strerror(errno));
        return false;
      }
      // wait for the child to exit
      if (waitid(P_PIDFD, mount->pidFd, &info, WEXITED) == -1) {
        logger::error("waitid() failed: {}", strerror(errno));
        return false;
      }
      logger::debug("usvfs exited with code {}", info.si_status);
    } else {
      fuse_unmount(mount->fusePtr);
      fuse_destroy(mount->fusePtr);
    }
  }
  m_mounts.clear();

  return true;
}

bool UsvfsManager::isMounted() const noexcept
{
  shared_lock lock(m_mtx);

  return m_mounts.empty();
}

void UsvfsManager::setUpperDir(std::string upperDir) noexcept
{
  scoped_lock lock(m_mtx);
  m_upperDir = std::move(upperDir);
}

void UsvfsManager::setUseMountNamespace(bool value) noexcept
{
  scoped_lock lock(m_mtx);
  m_useMountNamespace = value;
}

UsvfsManager::UsvfsManager() noexcept
{
  umask(0);

  auto logger = spdlog::get("usvfs");
  if (logger == nullptr) {
    logger = spdlog::create<spdlog::sinks::stdout_color_sink_mt>("usvfs");
    logger->set_pattern("%H:%M:%S.%e [%L] %v");
    logger->set_level(spdlog::level::info);
  }
}

void UsvfsManager::run_fuse(std::unique_ptr<MountState> state)
{
  unique_lock lock(state->mtx);
  const char* opts = m_debugMode ? "default_permissions,debug" : "default_permissions";
  const char* argv[] = {"usvfs_fuse", "-o", opts};
  int argc           = 3;
  fuse_args args     = FUSE_ARGS_INIT(argc, const_cast<char**>(argv));

  fuse_operations ops = createOperations();

  MountState* raw = state.get();
  raw->fusePtr    = fuse_new(&args, &ops, sizeof(fuse_operations), raw);
  fuse_opt_free_args(&args);
  if (!raw->fusePtr) {
    logger::error("fuse_new() failed for mountpoint {}", raw->mountpoint);
    raw->status = MountState::failure;
    lock.unlock();
    raw->cv.notify_all();
    return;
  }
  if (fuse_mount(raw->fusePtr, raw->mountpoint.c_str()) == -1) {
    fuse_destroy(raw->fusePtr);
    raw->fusePtr = nullptr;
    logger::error("fuse_mount() failed for mountpoint {}", raw->mountpoint);
    raw->status = MountState::failure;
    lock.unlock();
    raw->cv.notify_all();
    return;
  }

  m_mounts.emplace_back(std::move(state));

  raw->status = MountState::success;
  lock.unlock();
  raw->cv.notify_all();

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
  return ranges::any_of(m_spawnedProcesses, [&](const pid_t& pid) {
    int status;
    return waitpid(pid, &status, WNOHANG) > 0;
  });
}

bool UsvfsManager::mountInternal() noexcept
{
  if (m_pendingMounts.empty()) {
    return true;
  }

  logger::info("mounting {} mount points", m_pendingMounts.size());

  // move pending to a local list
  vector<unique_ptr<MountState>> toMount;
  toMount.swap(m_pendingMounts);

  int fd;
  if (!m_upperDir.empty()) {
    fd = open(m_upperDir.c_str(), OPEN_FLAGS);
    if (fd == -1) {
      logger::error("failed to open upper directory '{}': {}", m_upperDir,
                    strerror(errno));
      return false;
    }
  }

  // start a thread or process for each pending mount
  for (auto& state : toMount) {
    if (!m_upperDir.empty()) {
      state->upperDir = m_upperDir;
      logger::trace("adding fd {} for {}", fd, m_upperDir);
      state->fdMap[m_upperDir] = fd;
    }
    if (m_useMountNamespace) {
      // allocate memory to be used for the stack of the child.
      state->stack =
          static_cast<char*>(mmap(nullptr, stackSize, PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0));
      if (state->stack == MAP_FAILED) {
        logger::error("mmap() failed: {}", strerror(errno));
        return false;
      }

      state->stackTop = state->stack + stackSize;  // assume stack grows downward

      state->uid = getuid();
      state->gid = getgid();

      // only create a new namespace if m_nsPidFd == -1
      int flags;
      if (m_nsPidFd == -1) {
        flags = CLONE_NEWUSER | CLONE_NEWNS;
      } else {
        flags       = 0;
        state->nsFd = m_nsPidFd;
      }

      int result = clone(childFunc, state->stackTop,
                         flags | SIGCHLD | CLONE_PIDFD | CLONE_FILES | CLONE_VM,
                         state.get(), &state->pidFd);
      if (state->pidFd == -1 || result == -1) {
        logger::error("clone() failed: {}", strerror(errno));
        return false;
      }

      // check for error in child
      // todo: improve this to not not determine success with a timeout
      pollfd pfd = {state->pidFd, POLLIN, 0};
      result     = poll(&pfd, 1, pollTimeout);
      if (result == -1) {
        logger::error("poll() failed: {}", strerror(errno));
        return false;
      }
      if (result == 1) {
        const int e    = errno;
        siginfo_t info = {};
        if (waitid(P_PIDFD, state->pidFd, &info, WEXITED | WNOHANG) == -1 &&
            e != EAGAIN) {
          logger::error("waitid() failed: {}", strerror(errno));
          return false;
        }
        if (info.si_code != 0) {
          logger::error("child exited with status {}", info.si_code);
          return false;
        }
      }

      // store pid fd to access namespace
      if (m_nsPidFd == -1) {
        m_nsPidFd = state->pidFd;
      }

      logger::info("usvfs mounted in pid {}", pidfd_getpid(state->pidFd));
      m_mounts.emplace_back(std::move(state));
    } else {
      MountState* raw = state.get();

      thread t([s = std::move(state), this]() mutable {
        run_fuse(std::move(s));
      });
      t.detach();

      // wait until mount state is no longer unknown
      unique_lock lock(raw->mtx);
      raw->cv.wait(lock, [&] {
        return raw->status != MountState::unknown;
      });
      if (raw->status == MountState::failure) {
        logger::error("mount failed");
        return false;
      }

      logger::info("successfully mounted {}", raw->mountpoint);
    }
  }
  return true;
}
