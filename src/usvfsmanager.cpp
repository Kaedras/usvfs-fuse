#include "usvfs-fuse/usvfsmanager.h"

#include <QDir>

#include "fdmap.h"
#include "loghelpers.h"
#include "mountdata.h"
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

// mount status codes
constexpr int MOUNT_SUCCESS = 1;
constexpr int MOUNT_ERROR   = 2;

// child exit codes
constexpr int NAMESPACE_SETUP_FAILED = 1;
constexpr int SET_NS_FAILED          = 2;
constexpr int FUSE_NEW_FAILED        = 3;
constexpr int FUSE_MOUNT_FAILED      = 4;
constexpr int EVENTFD_WRITE_FAILED   = 5;
constexpr int EVENTFD_FAILED         = 6;

fuse_operations createOperations() noexcept
{
  fuse_operations ops = {};
  ops.getattr         = usvfs_getattr;
  ops.readlink        = usvfs_readlink;
  // ops.mknod
  ops.mkdir  = usvfs_mkdir;
  ops.unlink = usvfs_unlink;
  ops.rmdir  = usvfs_rmdir;
  // ops.symlink  = usvfs_symlink;
  ops.rename = usvfs_rename;
  // ops.link     = usvfs_link;
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

void readFromPipe(int fd, void* data, size_t size)
{
  char* ptr = static_cast<char*>(data);

  while (size > 0) {
    ssize_t readBytes = read(fd, ptr, size);
    if (readBytes == -1) {
      throw runtime_error("read error: "s + strerror(errno));
    }
    if (readBytes == 0) {
      throw runtime_error("read() returned 0");
    }

    ptr += readBytes;
    size -= readBytes;
  }
}

void writeToPipe(int fd, void* data, size_t size)
{
  char* ptr = static_cast<char*>(data);

  while (size > 0) {
    ssize_t writtenBytes = write(fd, ptr, size);
    if (writtenBytes == -1) {
      throw runtime_error("write error: "s + strerror(errno));
    }
    if (writtenBytes == 0) {
      throw runtime_error("write() returned 0");
    }
    ptr += writtenBytes;
    size -= writtenBytes;
  }
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
    if (eventfd_write(state->shutdownEventFd, MOUNT_ERROR) == -1) {
      spdlog::error("eventfd_write failed when notifying about mount error: {}",
                    strerror(errno));
    }
  };

  if (state->useMountNamespace) {
    // enter existing namespace
    if (state->nsFd != -1) {
      spdlog::debug("usvfs entering existing namespace, fd: {}", state->nsFd);
      int result = setns(state->nsFd, CLONE_NEWUSER | CLONE_NEWNS);
      if (result == -1) {
        spdlog::error("setns() failed: {}", strerror(errno));
        fail();
        return SET_NS_FAILED;
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
        return NAMESPACE_SETUP_FAILED;
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

  // create the fuse filesystem
  state->fusePtr = fuse_new(&args, &ops, sizeof(fuse_operations), state);
  fuse_opt_free_args(&args);
  if (state->fusePtr == nullptr) {
    spdlog::error("fuse_new() failed");
    fail();
    return FUSE_NEW_FAILED;
  }

  // mount the fuse filesystem
  if (fuse_mount(state->fusePtr, state->mountpoint.c_str()) == -1) {
    fuse_destroy(state->fusePtr);
    state->fusePtr = nullptr;
    spdlog::error("fuse_mount() failed for mountpoint {}: {}", state->mountpoint,
                  strerror(errno));
    fail();
    return FUSE_MOUNT_FAILED;
  }

  // notify parent
  spdlog::trace("mount success, notifying parent in fd {}", state->shutdownEventFd);
  if (eventfd_write(state->statusEventFd, MOUNT_SUCCESS) == -1) {
    spdlog::error("eventfd_write failed: {}", strerror(errno));
    fuse_destroy(state->fusePtr);
    state->fusePtr = nullptr;
    return EVENTFD_WRITE_FAILED;
  }

  // event fd to stop the worker threads
  const int stopEventFd = eventfd(0, EFD_NONBLOCK);
  if (stopEventFd == -1) {
    spdlog::error("eventfd() failed for stop event fd: {}", strerror(errno));
    return EVENTFD_FAILED;
  }

  // worker threads for unmounting and dumping the file tree
  // polling both a "do work" and "shutdown" event fd prevents several issues:
  // - poll() with timeout which slows down execution
  // - blocking indefinitely
  //
  // there is `fuse_set_signal_handlers()` to exit on HUP, TERM, and INT, but it does
  // not work reliably when using clone()

  jthread shutdownThread([state, stopEventFd](const stop_token& stoken) {
    stop_callback stopCallback(stoken, [stopEventFd]() {
      // write to stopEventFd to stop dumpThread
      eventfd_write(stopEventFd, 1);
    });

    pollfd fds[] = {
        {.fd = state->shutdownEventFd, .events = POLLIN, .revents = 0},
        {.fd = stopEventFd, .events = POLLIN, .revents = 0},
    };

    while (!stoken.stop_requested()) {
      const int result = poll(fds, size(fds), -1);
      if (result == -1) {
        if (errno == EINTR) {
          continue;
        }

        spdlog::error("poll error in shutdown thread: {}", strerror(errno));
        return;
      }

      if (fds[1].revents & POLLIN) {
        return;
      }

      if (fds[0].revents & POLLIN) {
        eventfd_t value;
        if (eventfd_read(state->shutdownEventFd, &value) == 0) {
          if (state->fusePtr != nullptr) {
            fuse_exit(state->fusePtr);
            fuse_unmount(state->fusePtr);
          }
          return;
        }
      }
    }
  });

  jthread dumpThread([state, stopEventFd](const stop_token& stoken) {
    try {
      pollfd fds[] = {
          {.fd = state->dumpEventFd, .events = POLLIN, .revents = 0},
          {.fd = stopEventFd, .events = POLLIN, .revents = 0},
      };

      while (!stoken.stop_requested()) {
        const int result = poll(fds, size(fds), -1);

        if (result == -1) {
          if (errno == EINTR) {
            continue;
          }

          spdlog::error("poll error in dump thread: {}", strerror(errno));
          return;
        }

        if (fds[1].revents & POLLIN) {
          return;
        }

        if (fds[0].revents & POLLIN) {
          eventfd_t value;
          if (eventfd_read(state->dumpEventFd, &value) == 0) {
            ostringstream oss;
            state->fileTree->dumpTree(oss);
            const string data = oss.str();

            size_t size = data.size();
            writeToPipe(state->dumpPipeFd, &size, sizeof(size_t));
            writeToPipe(state->dumpPipeFd, (void*)data.data(), data.size());
          }
        } else if (result == -1) {
          spdlog::error("poll error in dump thread: {}", strerror(errno));
          return;
        }
      }
    } catch (const runtime_error& ex) {
      spdlog::error("error in dump thread: {}", ex.what());
    }
  });

  // enter loop; this blocks until unmounted
  fuse_loop(state->fusePtr);

  // make sure the shutdown thread has finished before calling `fuse_destroy`
  shutdownThread.request_stop();
  shutdownThread.join();

  close(stopEventFd);

  fuse_destroy(state->fusePtr);
  state->fusePtr = nullptr;

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

void UsvfsManager::clearVirtualMappings() noexcept
{
  scoped_lock lock(m_mtx);
  m_pendingMounts.clear();
}

void UsvfsManager::virtualLinkFile(const std::string& source,
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

void UsvfsManager::virtualLinkDirectoryStatic(const std::string& source,
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

const std::vector<pid_t>& UsvfsManager::getVFSProcessList() const noexcept
{
  shared_lock lock(m_mtx);
  return m_spawnedProcesses;
}

pid_t UsvfsManager::createProcessHooked(const std::string& file, const std::string& arg,
                                        const std::string& workDir,
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

pid_t UsvfsManager::createProcessHooked(const QString& file,
                                        const QString& arg) noexcept
{
  return createProcessHooked(file, arg, QDir::currentPath());
}

pid_t UsvfsManager::createProcessHooked(const QString& file, const QString& arg,
                                        const QString& workDir,
                                        const QStringList& env) noexcept
{
  vector<string> env_;
  env_.reserve(env.size());
  for (const QString& value : env) {
    env_.emplace_back(value.toStdString());
  }

  return createProcessHooked(file.toStdString(), arg.toStdString(),
                             workDir.toStdString(), env_);
}

pid_t UsvfsManager::createProcessHooked(const std::string& file,
                                        const std::string& arg) noexcept
{
  char* cwd            = get_current_dir_name();
  const string workDir = cwd;
  free(cwd);
  return createProcessHooked(file, arg, workDir);
}

std::string UsvfsManager::createVFSDump() const noexcept
{
  shared_lock lock(m_mtx);
  ostringstream oss;
  spdlog::debug("dumping {} pending and {} active mounts", m_pendingMounts.size(),
                m_mounts.size());
  for (const auto& state : m_pendingMounts) {
    state->fileTree->dumpTree(oss);
  }
  string result = oss.str();

  for (const auto& state : m_mounts) {
    if (eventfd_write(state.dumpEventFd, 1) == -1) {
      spdlog::error("usvfsCreateVFSDump: eventfd_write failed on fd {}: {}",
                    state.dumpEventFd, strerror(errno));
      return {};
    }

    try {
      size_t size;
      readFromPipe(state.dumpPipeFd, &size, sizeof(size_t));
      string data;
      data.resize(size);
      readFromPipe(state.dumpPipeFd, data.data(), size);
      result.append(data);
    } catch (const runtime_error& ex) {
      spdlog::error("error reading from pipe: {}", ex.what());
      return {};
    }
  }

  return result;
}

void UsvfsManager::blacklistExecutable(const std::string& executableName) noexcept
{
  scoped_lock lock(m_mtx);
  spdlog::debug("blacklisting '{}'", executableName);
  m_executableBlacklist.emplace(executableName);
}

void UsvfsManager::clearExecutableBlacklist() noexcept
{
  scoped_lock lock(m_mtx);
  spdlog::debug("clearing blacklist");
  m_executableBlacklist.clear();
}

void UsvfsManager::addSkipFileSuffix(const std::string& fileSuffix) noexcept
{
  if (fileSuffix.empty()) {
    return;
  }

  scoped_lock lock(m_mtx);
  spdlog::debug("added skip file suffix '{}'", fileSuffix);
  m_skipFileSuffixes.emplace(fileSuffix);
}

void UsvfsManager::clearSkipFileSuffixes() noexcept
{
  scoped_lock lock(m_mtx);
  spdlog::debug("clearing skip file suffixes");
  m_skipFileSuffixes.clear();
}

void UsvfsManager::addSkipDirectory(const std::string& directory) noexcept
{
  if (directory.empty()) {
    return;
  }

  scoped_lock lock(m_mtx);
  spdlog::debug("added skip directory '{}'", directory);
  m_skipDirectories.emplace(directory);
}

void UsvfsManager::clearSkipDirectories() noexcept
{
  scoped_lock lock(m_mtx);
  spdlog::debug("clearing skip directories");
  m_skipDirectories.clear();
}

void UsvfsManager::forceLoadLibrary(const std::string& processName,
                                    const std::string& libraryPath) noexcept
{
  scoped_lock lock(m_mtx);
  spdlog::debug("adding forced library '{}' for process '{}'", libraryPath,
                processName);
  m_forceLoadLibraries.push_back({processName, libraryPath});
}

void UsvfsManager::clearLibraryForceLoads() noexcept
{
  scoped_lock lock(m_mtx);
  spdlog::debug("clearing forced libraries");
  m_forceLoadLibraries.clear();
}

void UsvfsManager::printDebugInfo() noexcept
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

const char* UsvfsManager::versionString() noexcept
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
      state->useMountNamespace = m_useMountNamespace;
      state->debugMode         = m_debugMode;

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
      char* stack =
          static_cast<char*>(mmap(nullptr, stackSize, PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0));
      if (stack == MAP_FAILED) {
        throw runtime_error("mmap() failed: "s + strerror(errno));
      }

      char* stackTop = stack + stackSize;  // assume stack grows downward

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

      // create event fds
      state->statusEventFd = eventfd(0, 0);
      if (state->statusEventFd == -1) {
        throw runtime_error("error creating status event fd: "s + strerror(errno));
      }

      state->shutdownEventFd = eventfd(0, 0);
      if (state->shutdownEventFd == -1) {
        throw runtime_error("error creating shutdown event fd: "s + strerror(errno));
      }

      state->dumpEventFd = eventfd(0, EFD_NONBLOCK);
      if (state->dumpEventFd == -1) {
        throw runtime_error("error creating dump event fd: "s + strerror(errno));
      }

      // create dump pipe
      int pipeFd[2];
      if (pipe(pipeFd) == -1) {
        throw runtime_error("error creating dump pipe: "s + strerror(errno));
      }
      state->dumpPipeFd = pipeFd[1];

      // create child process
      int result = clone(childFunc, stackTop, flags | SIGCHLD | CLONE_PIDFD,
                         state.get(), &state->pidFd);
      if (state->pidFd == -1 || result == -1) {
        throw runtime_error("clone() failed: "s + strerror(errno));
      }

      // check for error in child
      eventfd_t value;
      if (eventfd_read(state->statusEventFd, &value) != 0) {
        throw runtime_error("eventfd_read() failed: "s + strerror(errno));
      }

      if (value != MOUNT_SUCCESS) {
        throw runtime_error("error in mount process, status: " + to_string(value));
      }

      if (state->useMountNamespace) {
        // store pid fd to access namespace
        if (m_nsPidFd == -1) {
          m_nsPidFd = state->pidFd;
        }
      }

      spdlog::info("usvfs mounted in pid {}, mountpoint: '{}'",
                   pidfd_getpid(state->pidFd), state->mountpoint);
      m_mounts.emplace_back(state->pidFd, state->shutdownEventFd, state->dumpEventFd,
                            pipeFd[0], stack, state->mountpoint);
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

  for (auto& mount : m_mounts) {
    spdlog::debug("unmounting {}", mount.mountpoint);

    // unmount fuse
    if (mount.pidFd == -1) {
      spdlog::warn("mount pidFd is -1");
      return false;
    }

    // send shutdown event
    spdlog::trace("writing to event fd");
    if (eventfd_write(mount.shutdownEventFd, 1) == -1) {
      spdlog::error("unmount: eventfd_write failed: {}", strerror(errno));
      return false;
    }

    // wait for the child to exit
    pid_t pid = pidfd_getpid(mount.pidFd);
    spdlog::trace("waiting for pid {} to exit", pid);
    siginfo_t info{};
    if (waitid(P_PIDFD, mount.pidFd, &info, WEXITED | WSTOPPED) == -1) {
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
