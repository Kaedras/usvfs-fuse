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

  if (state->useMountNamespace) {
    // enter existing namespace
    if (state->nsFd != -1) {
      spdlog::debug("usvfs entering existing namespace, fd: {}", state->nsFd);
      int result = setns(state->nsFd, CLONE_NEWUSER | CLONE_NEWNS);
      if (result == -1) {
        spdlog::error("setns() failed: {}", strerror(errno));
        fail();
        return 1;
      }
    } else {
      try {
        // remap uid
        writeToFile("/proc/self/uid_map", format("0 {} 1\n", state->uid));
        // deny setgroups (see user_namespaces(7))
        writeToFile("/proc/self/setgroups", "deny\n");
        // remap gid
        writeToFile("/proc/self/gid_map", format("0 {} 1\n", state->gid));
      } catch (const exception& e) {
        spdlog::error("failed to set up namespace, {}", e.what());
        fail();
        return 1;
      }
    }
  }

  string opts = defaultMountOptions;
  if (state->debugMode) {
    opts.append(",debug");
  }
  const char* argv[] = {"usvfs_fuse", "-o", opts.c_str()};
  constexpr int argc = sizeof(argv) / sizeof(const char*);
  fuse_args args     = FUSE_ARGS_INIT(argc, const_cast<char**>(argv));

  fuse_operations ops = createOperations();

  *state->fusePtr = fuse_new(&args, &ops, sizeof(fuse_operations), state);
  fuse_opt_free_args(&args);
  if (*state->fusePtr == nullptr) {
    // Couldn't create FUSE handle; drop the mount
    spdlog::error("fuse_new() failed");
    fail();
    return 2;
  }
  if (fuse_mount(*state->fusePtr, state->mountpoint.c_str()) == -1) {
    fuse_destroy(*state->fusePtr);
    *state->fusePtr = nullptr;
    spdlog::error("fuse_mount() failed for mountpoint {}: {}", state->mountpoint,
                  strerror(errno));
    fail();
    return 3;
  }

  // set signal handlers
  fuse_session* session = fuse_get_session(*state->fusePtr);
  if (fuse_set_signal_handlers(session) == -1) {
    spdlog::error("fuse_set_signal_handlers() failed: {}", strerror(errno));
    fail();
    return 4;
  }

  spdlog::trace("mount success, notifying parent");
  {
    scoped_lock lock(state->statusData->mtx);
    state->statusData->status = MountState::success;
  }
  state->statusData->cv.notify_all();

  // enter loop; this blocks until unmounted or interrupted by the signal handler
  fuse_loop(*state->fusePtr);

  fuse_unmount(*state->fusePtr);
  fuse_destroy(*state->fusePtr);
  *state->fusePtr = nullptr;

  return 0;
}

size_t getProcessNamePosition(const string_view str)
{
  // todo: handle version suffixes, e.g. wine-staging-11.15
  constexpr array wineNames{"wine-staging"sv, "wine64-staging"sv, "wine"sv};

  for (const auto& wine : wineNames) {
    size_t position = str.find(wine);
    if (position != string::npos) {
      position += wine.length();
      // check for unexpected name or logic error
      if (str[position] != ' ') {
        spdlog::error("error in getProcessNamePosition(), expected ' ', got '{}'. "
                      "position: {}, found: {}, string: '{}'",
                      str[position], position, wine, str);
        return string::npos;
      }
      return position + 1;
    }
  }

  constexpr auto protonCommand = "/proton\" waitforexitandrun"sv;

  size_t position = str.find(protonCommand);
  if (position != string::npos) {
    position += protonCommand.length();
    // check for unexpected name or logic error
    if (str[position] != ' ') {
      spdlog::error("error in getProcessNamePosition(), expected ' ', got '{}'. "
                    "position: {}, string: '{}'",
                    str[position], position, str);
      return string::npos;
    }
    return position + 1;
  }

  return string::npos;
}

string_view getProcessName(string_view arg)
{
  // get start
  const size_t start = getProcessNamePosition(arg);
  if (start == string::npos) {
    return {};
  }

  // start + 1 to remove leading '"'
  arg.remove_prefix(start + 1);

  // find first non-escaped '"'
  size_t end = 0;
  while (end != string_view::npos) {
    end = arg.find('"', end);
    if (arg[end] - 1 == '\\') {
      ++end;
    } else {
      break;
    }
  }
  if (end == string_view::npos) {
    return {};
  }

  arg = arg.substr(0, end);

  const size_t lastSlash = arg.find_last_of('/');
  if (lastSlash == string::npos) {
    return {};
  }

  arg.remove_prefix(lastSlash + 1);
  spdlog::trace("using process name {}", arg);
  return arg;
}

}  // namespace

UsvfsManager::~UsvfsManager() noexcept
{
  unmount();
  if (m_nsPidFd != -1) {
    close(m_nsPidFd);
  }
}

void UsvfsManager::usvfsClearVirtualMappings() noexcept
{
  scoped_lock lock(m_mtx);
  m_pendingMounts.clear();
}

void UsvfsManager::usvfsVirtualLinkFile(const std::string& source,
                                        const std::string& destination) noexcept(false)
{
  scoped_lock lock(m_mtx);

  spdlog::trace("usvfsVirtualLinkFile(source='{}', destination='{}')", source,
                destination);

  if (fileNameInSkipSuffixes(getFileNameFromPath(source))) {
    spdlog::debug("file '{}' should be skipped", source);
    return;
  }

  const string src          = findCaseInsensitive(source);
  const string srcParentDir = getParentPath(source);
  const string dstParentDir = findCaseInsensitive(getParentPath(destination));
  const string dstFileName  = getFileNameFromPath(destination);

  MountState* existingState = nullptr;

  // check if destination exists in pending mounts
  for (const auto& state : m_pendingMounts) {
    if (isParentPathOf(state->mountpoint, dstParentDir)) {
      spdlog::debug("mountpoint already exists, adding to file tree");
      // destination exists, add to the existing file tree
      const string relative = destination.substr(state->mountpoint.length());
      auto result           = state->fileTree->add(relative, src, file, true);
      if (result != nullptr) {
        if (state->fdMap.add(srcParentDir) == -1) {
          throw runtime_error("error adding file descriptor for " + srcParentDir);
        }
      }
      if (result == nullptr) {
        const int e = errno;
        spdlog::error(format("error adding '{}' ({}) to file tree : ", relative, src,
                             strerror(e)));
      }
      return;
    }
    // check if the mountpoint is inside dstParentDir
    if (isParentPathOf(dstParentDir, state->mountpoint)) {
      spdlog::info("destination '{}' is a parent directory of existing mountpoint '{}'",
                   dstParentDir, state->mountpoint);

      existingState = state.get();
      break;
    }
  }

  // open a file descriptor for the source parent directory
  FdMap fdMap;
  if (fdMap.add(srcParentDir) == -1) {
    throw runtime_error("error adding file descriptor for " + srcParentDir);
  }

  // open a file descriptor for the destination parent directory
  if (fdMap.add(dstParentDir) == -1) {
    throw runtime_error("error adding file descriptor for " + dstParentDir);
  }

  // create the file tree for existing files
  shared_ptr<VirtualFileTreeItem> destinationFileTree =
      VirtualFileTreeItem::createFileTree(dstParentDir, fdMap);

  auto result = destinationFileTree->add(dstFileName, src, file, true);
  if (result == nullptr) {
    const int e = errno;
    throw runtime_error(
        format("error adding '{}' ({}) to file tree: ", dstFileName, src, strerror(e)));
  }

  if (existingState == nullptr) {
    // prepare state and enqueue to the pending list (no mounting yet)
    auto state        = make_unique<MountState>();
    state->fileTree   = std::move(destinationFileTree);
    state->mountpoint = dstParentDir;
    state->fdMap      = std::move(fdMap);
    m_pendingMounts.emplace_back(std::move(state));
  } else {
    // merge old file tree into destination file tree
    const string relative = existingState->mountpoint.substr(dstParentDir.length());
    const auto sourceItem = destinationFileTree->find(relative);
    if (!sourceItem) {
      const int e = errno;
      throw runtime_error(format("error merging destination '{}' with existing "
                                 "mountpoint '{}' (search path: '{}'): {}",
                                 destination, existingState->mountpoint, relative,
                                 strerror(e)));
    }
    sourceItem->merge(existingState->fileTree, false);
    existingState->fileTree = std::move(destinationFileTree);

    // update mountpoint
    existingState->mountpoint = dstParentDir;

    // merge fd maps
    existingState->fdMap.merge(fdMap);
  }
}

void UsvfsManager::usvfsVirtualLinkDirectoryStatic(const std::string& source,
                                                   const std::string& destination,
                                                   unsigned int flags) noexcept(false)
{
  scoped_lock lock(m_mtx);

  spdlog::trace("usvfsVirtualLinkDirectoryStatic(source='{}', destination='{}')",
                source, destination);

  const auto src = findCaseInsensitive(source);
  const auto dst = findCaseInsensitive(destination);

  error_code ec;

  // create destination if it does not exist
  if (!fs::exists(dst)) {
    fs::create_directories(dst, ec);
    if (ec) {
      throw runtime_error(
          format("error creating destination directory '{}': {}", dst, ec.message()));
    }
  }

  FdMap fdMap;
  if (fdMap.add(src) == -1) {
    throw runtime_error("error adding file descriptor for " + src);
  }

  // create the file tree
  auto sourceFileTree = VirtualFileTreeItem::create("/", src, dir);
  fs::recursive_directory_iterator iter(src, ec);
  if (ec) {
    throw runtime_error("error creating recursive_directory_iterator: "s +
                        ec.message());
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
      const int e = errno;
      throw runtime_error(
          format("error adding '{}' to file tree: {}", relative, strerror(e)));
    }
    if (isDirectory) {
      if (fdMap.add(entry.path().string()) == -1) {
        throw runtime_error("error adding file descriptor for " +
                            entry.path().string());
      }
    }
  }

  // check if destination exists in pending mounts
  for (const auto& state : m_pendingMounts) {
    // check if dst is inside the mountpoint
    if (isParentPathOf(state->mountpoint, dst)) {
      // destination exists, merge file trees
      spdlog::trace("merging destination '{}' into mountpoint '{}'", dst,
                    state->mountpoint);
      const string relative = dst.substr(state->mountpoint.length());
      auto existingItem     = state->fileTree->find(relative);
      if (existingItem == nullptr) {
        const int e = errno;
        throw runtime_error(
            format("Error merging destination '{}' into existing file tree: {}", dst,
                   strerror(e)));
      }
      existingItem->merge(sourceFileTree);
      state->fdMap.merge(fdMap);

      if (flags & linkFlag::CREATE_TARGET) {
        state->upperDir = src;
      }
      return;
    }
    // check if the mountpoint is inside dst
    if (isParentPathOf(dst, state->mountpoint)) {
      spdlog::trace("mountpoint '{}' is inside dst '{}'", state->mountpoint, dst);

      // create a new destination file tree
      shared_ptr<VirtualFileTreeItem> destinationFileTree =
          VirtualFileTreeItem::createFileTree(dst, fdMap);

      // merge old file tree into destination file tree
      const string relative = state->mountpoint.substr(dst.length());
      auto sourceItem       = destinationFileTree->find(relative);
      if (!sourceItem) {
        const int e = errno;
        throw runtime_error(format("error merging destination '{}' with existing "
                                   "mountpoint '{}' (search path: '{}'): {}",
                                   destination, state->mountpoint, relative,
                                   strerror(e)));
      }
      sourceItem->merge(state->fileTree, false);
      state->fileTree = std::move(destinationFileTree);

      // update mountpoint
      state->mountpoint = dst;

      // merge fd maps
      state->fdMap.merge(fdMap);

      if (flags & linkFlag::CREATE_TARGET) {
        state->upperDir = src;
      }
      return;
    }
  }

  // create the file tree for existing files
  shared_ptr<VirtualFileTreeItem> destinationFileTree =
      VirtualFileTreeItem::createFileTree(dst, fdMap);

  destinationFileTree->merge(sourceFileTree);

  // prepare state and add to the pending list (no mounting yet)
  auto state        = make_unique<MountState>();
  state->fileTree   = std::move(destinationFileTree);
  state->mountpoint = dst;
  state->fdMap      = std::move(fdMap);

  if (flags & linkFlag::CREATE_TARGET) {
    state->upperDir = src;
  }

  m_pendingMounts.emplace_back(std::move(state));
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

  spdlog::trace("usvfsCreateProcessHooked(file='{}', arg='{}', workdir='{}')", file,
                arg, workDir);

  if (!m_executableBlacklist.contains(file)) {
    if (!mountInternal()) {
      return -1;
    }
  }

  const string cmd = "'" + file + "' " + arg;
  spdlog::debug("usvfsCreateProcessHooked: command string: {}", cmd);

  int pipeFd[2];

  if (pipe(pipeFd) == -1) {
    spdlog::error("pipe failed: {}", strerror(errno));
    return -1;
  }

  const pid_t pid = fork();

  // error
  if (pid == -1) {
    close(pipeFd[0]);
    close(pipeFd[1]);

    spdlog::error("fork failed: {}", strerror(errno));
    return -1;
  }

  // child
  if (pid == 0) {
    // close read end
    close(pipeFd[0]);
    // set CLOEXEC on write end
    fcntl(pipeFd[1], F_SETFD, FD_CLOEXEC);

    if (m_useMountNamespace) {
      if (setns(m_nsPidFd, CLONE_NEWUSER | CLONE_NEWNS) == -1) {
        spdlog::error("setns failed: {}", strerror(errno));
        _exit(EXIT_FAILURE);
      }
    }

    if (chdir(workDir.c_str()) == -1) {
      spdlog::error("chdir('{}') failed: {}", workDir, strerror(errno));
      _exit(EXIT_FAILURE);
    }

    // handle wine dll overrides
    if (m_wine) {
      if (!m_forceLoadLibraries.empty()) {
        const string_view processName = getProcessName(arg);
        if (processName.empty()) {
          spdlog::error("error getting process name from '{}'", arg);
          _exit(EXIT_FAILURE);
        }
        spdlog::debug("using process name {}", processName);

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
    if (write(pipeFd[1], &error, sizeof(int)) == -1) {
      spdlog::error("Error writing exec error to pipe: {}\n Exec error was {}",
                    strerror(errno), strerror(error));
    }

    _exit(EXIT_FAILURE);
  }

  // parent

  // close write end
  close(pipeFd[1]);

  int error;
  size_t count = read(pipeFd[0], &error, sizeof(int));

  // close read end
  close(pipeFd[0]);

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
  return unmountInternal();
}

bool UsvfsManager::isMounted() const noexcept
{
  shared_lock lock(m_mtx);

  return !m_mounts.empty();
}

void UsvfsManager::setUseMountNamespace(bool value) noexcept
{
  scoped_lock lock(m_mtx);
  m_useMountNamespace = value;
}

void UsvfsManager::setWine(bool value) noexcept
{
  scoped_lock lock(m_mtx);
  m_wine = value;
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

bool UsvfsManager::fileNameInSkipSuffixes(
    const std::string_view fileName) const noexcept
{
  return fileNameInSkipSuffixes(fileName, m_skipFileSuffixes);
}

bool UsvfsManager::fileNameInSkipSuffixes(
    const std::string_view fileName, const std::set<std::string>& skipSuffixes) noexcept
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
    const std::string_view directoryName) const noexcept
{
  return fileNameInSkipDirectories(directoryName, m_skipDirectories);
}

bool UsvfsManager::fileNameInSkipDirectories(
    const std::string_view directoryName,
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

void UsvfsManager::reset() noexcept
{
  scoped_lock lock(m_mtx);

  unmountInternal();

  m_debugMode         = false;
  m_useMountNamespace = false;
  m_wine              = false;
  m_processDelay      = std::chrono::milliseconds::zero();

  m_skipFileSuffixes.clear();
  m_skipDirectories.clear();
  m_executableBlacklist.clear();
  m_forceLoadLibraries.clear();
  m_pendingMounts.clear();

  if (m_nsPidFd != -1) {
    close(m_nsPidFd);
    m_nsPidFd = -1;
  }

  m_spawnedProcesses.clear();
  m_logLevel = LogLevel::Debug;
}

std::vector<std::string>
UsvfsManager::librariesToForceLoad(const std::string_view processName) const noexcept
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
  try {
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

      // create shared memory for status data.
      raw->statusData = static_cast<MountState::StatusData*>(
          mmap(nullptr, sizeof(MountState::StatusData), PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_ANONYMOUS, -1, 0));
      if (raw->statusData == MAP_FAILED) {
        throw runtime_error("error creating shared memory for mount state: "s +
                            strerror(errno));
      }

      // create shared memory for fuse ptr.
      raw->fusePtr =
          static_cast<fuse**>(mmap(nullptr, sizeof(fuse*), PROT_READ | PROT_WRITE,
                                   MAP_SHARED | MAP_ANONYMOUS, -1, 0));
      if (raw->fusePtr == MAP_FAILED) {
        throw runtime_error("error creating shared memory for mount state: "s +
                            strerror(errno));
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
        throw runtime_error("mmap() failed: "s + strerror(errno));
      }

      state->stackTop = state->stack + stackSize;  // assume stack grows downward

      state->uid = getuid();
      state->gid = getgid();

      int flags = 0;
      if (state->useMountNamespace) {
        // only create a new namespace if m_nsPidFd == -1
        if (m_nsPidFd == -1) {
          flags = CLONE_NEWUSER | CLONE_NEWNS;
        } else {
          state->nsFd = m_nsPidFd;
        }
      }

      int result = clone(childFunc, state->stackTop,
                         flags | SIGCHLD | CLONE_PIDFD | CLONE_FILES | CLONE_VM,
                         state.get(), &state->pidFd);
      if (state->pidFd == -1 || result == -1) {
        throw runtime_error("clone() failed: "s + strerror(errno));
      }

      // check for error in child
      unique_lock lock(raw->statusData->mtx);
      raw->statusData->cv.wait(lock, [&] {
        return raw->statusData->status != MountState::unknown;
      });
      if (raw->statusData->status == MountState::failure) {
        throw runtime_error("error in mount process");
      }

      // store pid fd to access namespace
      if (m_nsPidFd == -1) {
        m_nsPidFd = state->pidFd;
      }

      spdlog::info("usvfs mounted in pid {}, mountpoint: '{}'",
                   pidfd_getpid(state->pidFd), state->mountpoint);
      m_mounts.emplace_back(std::move(state));
    }
    return true;
  } catch (const runtime_error& e) {
    spdlog::error("mount error: {}", e.what());

    if (!m_mounts.empty()) {
      spdlog::info("cleaning up partial mount");

      if (!unmountInternal()) {
        spdlog::critical("error cleaning up partial mount");
      }
    }

    return false;
  }
}

bool UsvfsManager::unmountInternal() noexcept
{
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
    if (mount->pidFd == -1) {
      spdlog::warn("mount pidFd is -1");
      return false;
    }

    pid_t pid = pidfd_getpid(mount->pidFd);
    spdlog::trace("sending SIGINT to pid {}", pid);
    if (pidfd_send_signal(mount->pidFd, SIGINT, nullptr, 0) == -1) {
      spdlog::error("pidfd_send_signal() failed: {}", strerror(errno));
      return false;
    }

    fuse_exit(*mount->fusePtr);

    // wait for the child to exit
    spdlog::trace("waiting for pid {} to exit", pid);
    siginfo_t info{};
    if (waitid(P_PIDFD, mount->pidFd, &info, WEXITED | WSTOPPED) == -1) {
      spdlog::error("waitid() failed: {}", strerror(errno));
      return false;
    }

    if (info.si_code == CLD_EXITED && info.si_status != 0) {
      spdlog::error("usvfs has exited with status {}", info.si_status);
    } else {
      string action;
      switch (info.si_code) {
      case CLD_EXITED:
        action = "has exited";
        break;
      case CLD_STOPPED:
        action = "has stopped";
        break;
      case CLD_KILLED:
        action = "was killed";
        break;
      default:
        action = "exited with code " + to_string(info.si_code) + " and";
      }
      spdlog::info("usvfs {} with status {}", action, info.si_status);
    }
  }
  m_mounts.clear();

  return true;
}
