#include "usvfs-fuse/usvfsmanager.h"

#include <QDir>

#include "fdmap.h"
#include "loghelpers.h"
#include "mountstate.h"
#include "usvfs-fuse/usvfs_version.h"
#include "usvfs.h"
#include "utils.h"
#include "virtualfiletreeitem.h"

using namespace std;
namespace fs = std::filesystem;

namespace
{

constexpr size_t defaultStackSize  = 1024 * 1024 * 8;  // stack size for cloned child.
constexpr auto defaultMountOptions = "default_permissions,auto_unmount";
const string logPattern            = "%H:%M:%S.%e [%L] %v";

fuse_operations createOperations() noexcept
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
  ops.readdir = usvfs_readdir;
  // ops.releasedir = usvfs_releasedir;
  ops.fsyncdir = usvfs_fsyncdir;
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

void writeToFile(const string& filename, string_view content) noexcept(false)
{
  ofstream ofs(filename);
  if (!ofs) {
    spdlog::error("failed to open file '{}': {}", filename, strerror(errno));
  }
  ofs.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  ofs << content;
}

int childFunc(void* arg) noexcept
{
  auto* state = static_cast<MountState*>(arg);

  auto fail = [&]() {
    {
      scoped_lock lock(state->statusData->mtx);
      state->statusData->status = MountState::failure;
    }
    state->statusData->cv.notify_all();
  };

  try {
    // remap uid
    writeToFile("/proc/self/uid_map", format("0 {} 1", state->uid));
    // deny setgroups (see user_namespaces(7))
    writeToFile("/proc/self/setgroups", "deny");
    // remap gid
    writeToFile("/proc/self/gid_map", format("0 {} 1", state->gid));
  } catch (const exception& e) {
    spdlog::error("failed to set up namespace, {}", e.what());
    fail();
    return -1;
  }

  // enter existing namespace
  if (state->nsFd != -1) {
    spdlog::debug("usvfs entering existing namespace");
    int result = setns(state->nsFd, CLONE_NEWUSER | CLONE_NEWNS);
    if (result == -1) {
      spdlog::error("setns() failed: {}", strerror(errno));
      fail();
      return -1;
    }
  }

  string opts = defaultMountOptions;
  if (state->debugMode) {
    opts.append(",debug");
  }
  const char* argv[] = {"usvfs_fuse", "-o", opts.c_str()};
  int argc           = 3;
  fuse_args args     = FUSE_ARGS_INIT(argc, const_cast<char**>(argv));

  fuse_operations ops = createOperations();

  state->fusePtr = fuse_new(&args, &ops, sizeof(fuse_operations), state);
  fuse_opt_free_args(&args);
  if (state->fusePtr == nullptr) {
    // Couldn't create FUSE handle; drop the mount
    spdlog::error("fuse_new() failed");
    fail();
    return -1;
  }
  if (fuse_mount(state->fusePtr, state->mountpoint.c_str()) == -1) {
    fuse_destroy(state->fusePtr);
    state->fusePtr = nullptr;
    spdlog::error("fuse_mount() failed for mountpoint {}: {}", state->mountpoint,
                  strerror(errno));
    fail();
    return -1;
  }

  // set signal handlers
  fuse_session* session = fuse_get_session(state->fusePtr);
  if (fuse_set_signal_handlers(session) == -1) {
    spdlog::error("fuse_set_signal_handlers() failed: {}", strerror(errno));
    fail();
    return -1;
  }

  spdlog::trace("mount success, notifying parent");
  {
    scoped_lock lock(state->statusData->mtx);
    state->statusData->status = MountState::success;
  }
  state->statusData->cv.notify_all();

  // enter loop; this blocks until unmounted or interrupted by the signal handler
  fuse_loop(state->fusePtr);

  fuse_unmount(state->fusePtr);
  fuse_destroy(state->fusePtr);
  state->fusePtr = nullptr;

  return 0;
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
                                        const std::string& destination) noexcept
{
  scoped_lock lock(m_mtx);

  spdlog::trace("{}, source: {}, destination: {}", __FUNCTION__, source, destination);

  if (fileNameInSkipSuffixes(getFileNameFromPath(source))) {
    spdlog::debug("file {} should be skipped", source);
    return true;
  }

  const string src          = findCaseInsensitive(source);
  const string srcParentDir = getParentPath(source);
  const string dstParentDir = getParentPath(destination);
  const string dstFileName  = getFileNameFromPath(destination);

  // check if destination exists in pending mounts
  for (const auto& state : m_pendingMounts) {
    if (isParentPathOf(state->mountpoint, dstParentDir)) {
      spdlog::debug("mountpoint already exists, adding to file tree");
      // destination exists, add to the existing file tree
      const string relative = destination.substr(state->mountpoint.length());
      auto result           = state->fileTree->add(relative, src, file, true);
      if (result != nullptr) {
        if (state->fdMap.add(srcParentDir) == -1) {
          return false;
        }
      }
      return result != nullptr;
    }
  }

  // open a file descriptor for the source parent directory
  FdMap fdMap;
  if (fdMap.add(srcParentDir) == -1) {
    return false;
  }

  // open a file descriptor for the destination parent directory
  if (fdMap.add(dstParentDir) == -1) {
    return false;
  }

  // create the file tree for existing files
  shared_ptr<VirtualFileTreeItem> destinationFileTree =
      VirtualFileTreeItem::createFileTree(dstParentDir, fdMap);

  auto result = destinationFileTree->add(dstFileName, source, file, true);
  if (result == nullptr) {
    return false;
  }

  // prepare state and enqueue to the pending list (no mounting yet)
  auto state        = make_unique<MountState>();
  state->fileTree   = std::move(destinationFileTree);
  state->mountpoint = dstParentDir;
  state->fdMap      = std::move(fdMap);
  m_pendingMounts.emplace_back(std::move(state));

  return true;
}

bool UsvfsManager::usvfsVirtualLinkDirectoryStatic(const std::string& source,
                                                   const std::string& destination,
                                                   unsigned int flags) noexcept
{
  scoped_lock lock(m_mtx);

  spdlog::trace("{}, source: {}, destination: {}", __FUNCTION__, source, destination);

  const auto src = findCaseInsensitive(source);
  const auto dst = findCaseInsensitive(destination);

  FdMap fdMap;
  if (fdMap.add(src) == -1) {
    return false;
  }

  // create the file tree
  auto sourceFileTree = VirtualFileTreeItem::create("/", src, dir);
  error_code ec;
  fs::recursive_directory_iterator iter(src, ec);
  if (ec) {
    spdlog::error("error creating recursive_directory_iterator: {}", ec.message());
    return false;
  }
  for (const fs::directory_entry& entry : iter) {
    // check if the directory should be skipped
    string fileName = entry.path().filename().string();
    if ((entry.is_directory() && fileNameInSkipDirectories(fileName)) ||
        (!entry.is_directory() && fileNameInSkipSuffixes(fileName))) {
      continue;
    }
    const string entryPath = entry.path().string();
    const bool isDirectory = entry.is_directory();

    string_view relative = relativePath(entryPath, src);

    spdlog::trace("adding '{}' to file tree", relative);
    auto newItem =
        sourceFileTree->add(relative, entry.path().string(), isDirectory ? dir : file);
    if (newItem == nullptr) {
      spdlog::error("error adding '{}' to file tree", relative);
      return false;
    }
    if (isDirectory) {
      if (fdMap.add(entry.path().string()) == -1) {
        return false;
      }
    }
  }

  // check if destination exists in pending mounts
  for (const auto& state : m_pendingMounts) {
    if (isParentPathOf(state->mountpoint, destination)) {
      // destination exists, merge file trees
      const string relative = destination.substr(state->mountpoint.length());
      auto existingItem     = state->fileTree->find(relative);
      if (existingItem == nullptr) {
        spdlog::error("Error merging destination '{}' into existing file tree",
                      destination);
        return false;
      }
      existingItem->merge(sourceFileTree);
      state->fdMap.merge(fdMap);

      if (flags & linkFlag::CREATE_TARGET) {
        state->upperDir = source;
      }
      return true;
    }
  }

  // create the file tree for existing files
  shared_ptr<VirtualFileTreeItem> destinationFileTree =
      VirtualFileTreeItem::createFileTree(destination, fdMap);

  destinationFileTree->merge(sourceFileTree);

  // prepare state and add to the pending list (no mounting yet)
  auto state        = make_unique<MountState>();
  state->fileTree   = std::move(destinationFileTree);
  state->mountpoint = destination;
  state->fdMap      = std::move(fdMap);

  if (flags & linkFlag::CREATE_TARGET) {
    state->upperDir = source;
  }

  m_pendingMounts.emplace_back(std::move(state));

  return true;
}

const std::vector<pid_t>& UsvfsManager::usvfsGetVFSProcessList() const noexcept
{
  shared_lock lock(m_mtx);
  return m_spawnedProcesses;
}

pid_t UsvfsManager::usvfsCreateProcessHooked(
    const std::string& file, const std::string& arg, const std::string& workDir,
    const std::vector<std::string>& env) noexcept
{
  scoped_lock lock(m_mtx);

  // sanity check
  if (!m_mounts.empty() && m_nsPidFd == -1) {
    spdlog::error("usvfs is mounted without any reference to a namespace, aborting");
    return false;
  }

  spdlog::trace("{}: {}, {}, {}", __FUNCTION__, file, arg, workDir);

  if (!m_executableBlacklist.contains(file)) {
    if (!mountInternal()) {
      return -1;
    }
  }

  const string cmd = "'" + file + "' " + arg;
  spdlog::debug("{}: command string: {}", __FUNCTION__, cmd);

  int pipefd[2];

  if (pipe(pipefd) == -1) {
    spdlog::error("pipe failed: {}", strerror(errno));
    return -1;
  }

  const pid_t pid = fork();

  // error
  if (pid == -1) {
    close(pipefd[0]);
    close(pipefd[1]);

    spdlog::error("fork failed: {}", strerror(errno));
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
        spdlog::error("setns failed: {}", strerror(errno));
        exit(EXIT_FAILURE);
      }
    }

    if (chdir(workDir.c_str()) == -1) {
      spdlog::error("chdir failed: {}", strerror(errno));
    }

    // handle wine dll overrides
    const bool wine   = file.ends_with("wine") || file.ends_with("wine-staging") ||
                        file.ends_with("wine64") || file.ends_with("wine64-staging");
    const bool proton = file.ends_with("proton");

    if (wine || proton) {
      if (!m_forceLoadLibraries.empty()) {
        const size_t firstSpace = arg.find_first_of(' ');
        const string processName =
            wine ? arg.substr(0, firstSpace - 1)
                 : arg.substr(firstSpace, arg.find_first_of(' ') - 1);
        spdlog::trace("using process name {}", processName);
        const vector<string> applicableLibraries = librariesToForceLoad(processName);
        if (!applicableLibraries.empty()) {
          string dllOverrides = "WINEDLLOVERRIDES=\"";
          for (size_t i = 0; i < applicableLibraries.size() - 1; ++i) {
            dllOverrides += applicableLibraries[i] + "=n,b;";
          }
          dllOverrides += applicableLibraries.back() + "=n,b\"";
          putenv(const_cast<char*>(dllOverrides.c_str()));
          spdlog::debug("adding '{}' to process", dllOverrides);
        }
      }
    }

    for (const string& entry : env) {
      putenv(const_cast<char*>(entry.c_str()));
    }

    execl("/bin/sh", "/bin/sh", "-c", cmd.c_str(), nullptr);

    // write error to pipe
    const int error = errno;
    if (write(pipefd[1], &error, sizeof(int)) == -1) {
      spdlog::error("Error writing exec error to pipe: {}\n Exec error was {}",
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

  spdlog::error("execl failed: {}", strerror(error));
  return -1;
}

pid_t UsvfsManager::usvfsCreateProcessHooked(const QString& file,
                                             const QString& arg) noexcept
{
  return usvfsCreateProcessHooked(file, arg, QDir::currentPath());
}

pid_t UsvfsManager::usvfsCreateProcessHooked(const QString& file, const QString& arg,
                                             const QString& workDir,
                                             const QStringList& env) noexcept
{
  vector<string> env_;
  env_.reserve(env.size());
  for (const QString& value : env) {
    env_.emplace_back(value.toStdString());
  }

  return usvfsCreateProcessHooked(file.toStdString(), arg.toStdString(),
                                  workDir.toStdString(), env_);
}

pid_t UsvfsManager::usvfsCreateProcessHooked(const std::string& file,
                                             const std::string& arg) noexcept
{
  char* cwd            = get_current_dir_name();
  const string workDir = cwd;
  free(cwd);
  return usvfsCreateProcessHooked(file, arg, workDir);
}

std::string UsvfsManager::usvfsCreateVFSDump() const noexcept
{
  shared_lock lock(m_mtx);
  ostringstream oss;
  string result;
  spdlog::debug("dumping {} pending and {} active mounts", m_pendingMounts.size(),
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
  spdlog::debug("blacklisting '{}'", executableName);
  m_executableBlacklist.emplace(executableName);
}

void UsvfsManager::usvfsClearExecutableBlacklist() noexcept
{
  scoped_lock lock(m_mtx);
  spdlog::debug("clearing blacklist");
  m_executableBlacklist.clear();
}

void UsvfsManager::usvfsAddSkipFileSuffix(const std::string& fileSuffix) noexcept
{
  if (fileSuffix.empty()) {
    return;
  }

  scoped_lock lock(m_mtx);
  spdlog::debug("added skip file suffix '{}'", fileSuffix);
  m_skipFileSuffixes.emplace(fileSuffix);
}

void UsvfsManager::usvfsClearSkipFileSuffixes() noexcept
{
  scoped_lock lock(m_mtx);
  spdlog::debug("clearing skip file suffixes");
  m_skipFileSuffixes.clear();
}

void UsvfsManager::usvfsAddSkipDirectory(const std::string& directory) noexcept
{
  if (directory.empty()) {
    return;
  }

  scoped_lock lock(m_mtx);
  spdlog::debug("added skip directory '{}'", directory);
  m_skipDirectories.emplace(directory);
}

void UsvfsManager::usvfsClearSkipDirectories() noexcept
{
  scoped_lock lock(m_mtx);
  spdlog::debug("clearing skip directories");
  m_skipDirectories.clear();
}

void UsvfsManager::usvfsForceLoadLibrary(const std::string& processName,
                                         const std::string& libraryPath) noexcept
{
  scoped_lock lock(m_mtx);
  spdlog::debug("adding forced library '{}' for process '{}'", libraryPath,
                processName);
  m_forceLoadLibraries.push_back({processName, libraryPath});
}

void UsvfsManager::usvfsClearLibraryForceLoads() noexcept
{
  scoped_lock lock(m_mtx);
  spdlog::debug("clearing forced libraries");
  m_forceLoadLibraries.clear();
}

void UsvfsManager::usvfsPrintDebugInfo() noexcept
{
#warning STUB
  // spdlog::get("usvfs")->warn("===== debug {} =====",
  //                            context->redirectionTable().shmName());
  // void* buffer      = nullptr;
  // size_t bufferSize = 0;
  // context->redirectionTable().getBuffer(buffer, bufferSize);
  // std::ostringstream temp;
  // for (size_t i = 0; i < bufferSize; ++i) {
  //   temp << std::hex << std::setfill('0') << std::setw(2)
  //        << (unsigned)reinterpret_cast<char*>(buffer)[i] << " ";
  //   if ((i % 16) == 15) {
  //     spdlog::get("usvfs")->info("{}", temp.str());
  //     temp.str("");
  //     temp.clear();
  //   }
  // }
  // if (!temp.str().empty()) {
  //   spdlog::get("usvfs")->info("{}", temp.str());
  // }
  // spdlog::get("usvfs")->warn("===== / debug {} =====",
  //                            context->redirectionTable().shmName());
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
  m_logLevel = logLevel;
  spdlog::set_level(ConvertLogLevel(logLevel));
}

void UsvfsManager::setLogFile(const std::string& logFile) noexcept
{
  scoped_lock lock(m_mtx);

  auto stdoutSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  auto fileSink   = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile);

  stdoutSink->set_pattern(logPattern);
  fileSink->set_pattern(logPattern);
  stdoutSink->set_level(spdlog::level::info);
  fileSink->set_level(spdlog::level::trace);

  vector<spdlog::sink_ptr> sinks{stdoutSink, fileSink};
  auto logger = std::make_shared<spdlog::logger>("usvfs", sinks.begin(), sinks.end());
  logger->set_level(ConvertLogLevel(m_logLevel));
  spdlog::set_default_logger(logger);
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

  spdlog::info("unmounting {} mounts", m_mounts.size());

  if (anyProcessRunning()) {
    spdlog::warn("there is still at least one process running, not unmounting");
    return false;
  }

  for (std::unique_ptr<MountState>& mount : m_mounts) {
    spdlog::debug("unmounting {}", mount->mountpoint);

    // unmount fuse
    if (m_useMountNamespace) {
      if (mount->pidFd == -1) {
        spdlog::warn("mount pidFd is -1");
        return false;
      }

      siginfo_t info;
      if (pidfd_send_signal(mount->pidFd, SIGINT, nullptr, 0) == -1) {
        spdlog::error("pidfd_send_signal() failed: {}", strerror(errno));
        return false;
      }
      // wait for the child to exit
      if (waitid(P_PIDFD, mount->pidFd, &info, WEXITED) == -1) {
        spdlog::error("waitid() failed: {}", strerror(errno));
        return false;
      }
      spdlog::debug("usvfs exited with code {}", info.si_status);
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

void UsvfsManager::setUseMountNamespace(bool value) noexcept
{
  scoped_lock lock(m_mtx);
  m_useMountNamespace = value;
}

UsvfsManager::UsvfsManager() noexcept
{
  umask(0);
  maximizeFdLimit();

  auto logger = spdlog::create<spdlog::sinks::stdout_color_sink_mt>("usvfs");
  logger->set_pattern(logPattern);
  logger->set_level(spdlog::level::debug);
  spdlog::set_default_logger(logger);
}

void UsvfsManager::run_fuse(std::unique_ptr<MountState> state)
{
  string opts = defaultMountOptions;
  if (state->debugMode) {
    opts.append(",debug");
  }
  const char* argv[] = {"usvfs_fuse", "-o", opts.c_str()};
  int argc           = 3;
  fuse_args args     = FUSE_ARGS_INIT(argc, const_cast<char**>(argv));

  fuse_operations ops = createOperations();

  MountState* raw = state.get();

  auto fail = [&]() {
    {
      scoped_lock lock(state->statusData->mtx);
      raw->statusData->status = MountState::failure;
    }
    raw->statusData->cv.notify_all();
  };

  raw->fusePtr = fuse_new(&args, &ops, sizeof(fuse_operations), raw);
  fuse_opt_free_args(&args);
  if (!raw->fusePtr) {
    spdlog::error("fuse_new() failed for mountpoint {}", raw->mountpoint);
    fail();
    return;
  }
  if (fuse_mount(raw->fusePtr, raw->mountpoint.c_str()) == -1) {
    fuse_destroy(raw->fusePtr);
    raw->fusePtr = nullptr;
    spdlog::error("fuse_mount() failed for mountpoint {}", raw->mountpoint);
    fail();
    return;
  }

  m_mounts.emplace_back(std::move(state));

  {
    scoped_lock lock(raw->statusData->mtx);
    raw->statusData->status = MountState::success;
  }
  raw->statusData->cv.notify_all();

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
      spdlog::debug("file '{}' should be skipped, matches file suffix '{}'", fileName,
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
      spdlog::debug("directory '{}' should be skipped", directoryName);
      return true;
    }
    return false;
  });
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

  spdlog::info("mounting {} mount points", m_pendingMounts.size());

  // move pending to a local list
  vector<unique_ptr<MountState>> toMount;
  toMount.swap(m_pendingMounts);

  // start a thread or process for each pending mount
  for (auto& state : toMount) {
    MountState* raw = state.get();

    raw->useMountNamespace = m_useMountNamespace;
    raw->debugMode         = m_debugMode;

    if (m_useMountNamespace) {
      // create shared memory for status data.
      raw->statusData = static_cast<MountState::StatusData*>(
          mmap(nullptr, sizeof(MountState::StatusData), PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_ANONYMOUS, -1, 0));
      if (raw->statusData == MAP_FAILED) {
        spdlog::error("error creating shared memory for mount state: {}",
                      strerror(errno));
        return false;
      }

      size_t stackSize;
      rlimit limit;
      if (getrlimit(RLIMIT_STACK, &limit) == 0) {
        stackSize = min(limit.rlim_cur, defaultStackSize);
        spdlog::trace("setting stack size to {}", stackSize);
      } else {
        stackSize = defaultStackSize;
        spdlog::trace("error getting stack size limit: {}\nusing 8 MiB",
                      strerror(errno));
      }

      // allocate memory to be used for the stack of the child.
      state->stack =
          static_cast<char*>(mmap(nullptr, stackSize, PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0));
      if (state->stack == MAP_FAILED) {
        spdlog::error("mmap() failed: {}", strerror(errno));
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
        spdlog::error("clone() failed: {}", strerror(errno));
        return false;
      }

      // check for error in child
      unique_lock lock(raw->statusData->mtx);
      raw->statusData->cv.wait(lock, [&] {
        return raw->statusData->status != MountState::unknown;
      });
      if (raw->statusData->status == MountState::failure) {
        spdlog::error("mount failed");
        return false;
      }

      // store pid fd to access namespace
      if (m_nsPidFd == -1) {
        m_nsPidFd = state->pidFd;
      }

      spdlog::info("usvfs mounted in pid {}", pidfd_getpid(state->pidFd));
      m_mounts.emplace_back(std::move(state));
    } else {
      try {
        raw->statusData = new MountState::StatusData();
      } catch (const bad_alloc& ex) {
        spdlog::error("error allocating memory for mount state: {}", ex.what());
        return false;
      }
      thread t([s = std::move(state), this]() mutable {
        run_fuse(std::move(s));
      });
      t.detach();

      // wait until mount state is no longer unknown
      unique_lock lock(raw->statusData->mtx);
      raw->statusData->cv.wait(lock, [&] {
        return raw->statusData->status != MountState::unknown;
      });
      if (raw->statusData->status == MountState::failure) {
        spdlog::error("mount failed");
        return false;
      }

      spdlog::info("successfully mounted {}", raw->mountpoint);
    }
  }
  return true;
}
