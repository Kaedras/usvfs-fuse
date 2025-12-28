#pragma once

#include "logging.h"
#include <shared_mutex>

// forward declarations
struct MountState;
class VirtualFileTreeItem;
class QProcess;

namespace spdlog
{
class logger;
namespace sinks
{
  class sink;
}
}  // namespace spdlog

struct fuse_file_info;
struct fuse_operations;
struct fuse;

namespace linkFlag
{
static constexpr unsigned int FAIL_IF_EXISTS =
    0x00000001;  // if set, linking fails in case of an error
static constexpr unsigned int MONITOR_CHANGES =
    0x00000002;  // if set, changes to the source directory after the link operation
// will be updated in the virtual fs. only relevant in static
// link directory operations
static constexpr unsigned int CREATE_TARGET =
    0x00000004;  // if set, file creation (including move or copy) operations to
// destination will be redirected to the source. Only one createtarget
// can be set for a destination folder so this flag will replace
// the previous create target.
// If there different create-target have been set for an element and
// one of its ancestors, the inner-most create-target is used
static constexpr unsigned int RECURSIVE =
    0x00000008;  // if set, directories are linked recursively
static constexpr unsigned int FAIL_IF_SKIPPED =
    0x00000010;  // if set, linking fails if the file or directory is skipped
// files or directories are skipped depending on whats been added to
// the skip file suffixes or skip directories list in
// the sharedparameters class, those lists are checked during virtual
// linking
}  // namespace linkFlag

class __attribute__((visibility("default"))) UsvfsManager
{
public:
  UsvfsManager(UsvfsManager const&)            = delete;
  UsvfsManager& operator=(UsvfsManager const&) = delete;

  static std::shared_ptr<UsvfsManager> instance() noexcept
  {
    static std::shared_ptr<UsvfsManager> s{new UsvfsManager};
    return s;
  }

  ~UsvfsManager() noexcept;

  /**
   * removes all virtual mappings
   */
  void usvfsClearVirtualMappings() noexcept;

  /**
   * link a file virtually
   * @note: the directory the destination file resides in has to exist - at least
   * virtually.
   */
  bool usvfsVirtualLinkFile(const char* source, const char* destination,
                            unsigned int flags) noexcept;

  /**
   * link a directory virtually. This static variant recursively links all files
   * individually, change notifications are used to update the information.
   * @param failIfExists if true, this call fails if the destination directory exists
   * (virtually or physically)
   */
  bool usvfsVirtualLinkDirectoryStatic(const char* source, const char* destination,
                                       unsigned int flags) noexcept;

  /**
   * connect to a virtual filesystem as a controller, without hooking the calling
   * process. Please note that you can only be connected to one vfs, so this will
   * silently disconnect from a previous vfs.
   */
  // bool usvfsConnectVFS(const usvfsParameters* p);

  /**
   * @brief create a new VFS. This is similar to ConnectVFS except it guarantees
   *   the vfs is reset before use.
   */
  // bool usvfsCreateVFS(const usvfsParameters* p);

  // std::string usvfsGetCurrentVFSName();

  /**
   * disconnect from a virtual filesystem. This removes hooks if necessary
   */
  // void usvfsDisconnectVFS();

  /**
   * retrieve a list of all processes connected to the vfs
   */
  std::vector<std::unique_ptr<QProcess>>& usvfsGetVFSProcessList() noexcept;

  // retrieve a list of all processes connected to the vfs, stores an array
  // of `count` elements in `*buffer`
  //
  // if this returns TRUE and `count` is not 0, the caller must release the buffer
  // with `free(*buffer)`
  //
  // return values:
  //   - ERROR_INVALID_PARAMETERS:  either `count` or `buffer` is NULL
  //   - ERROR_TOO_MANY_OPEN_FILES: there seems to be way too many usvfs processes
  //                                running, probably some internal error
  //   - ERROR_NOT_ENOUGH_MEMORY:   malloc() failed
  //
  // bool usvfsGetVFSProcessList2(size_t* count, uint32_t** buffer);

  /**
   * spawn a new process that can see the virtual file system.
   */
  bool usvfsCreateProcessHooked(const char* file, const char* arg, const char* workDir,
                                char** envp) noexcept;
  bool usvfsCreateProcessHooked(const char* file, const char* arg,
                                const char* workDir) noexcept;
  bool usvfsCreateProcessHooked(const char* file, const char* arg) noexcept;

  /**
   * retrieve a single log message.
   * FIXME There is currently no way to unblock from the caller side
   * FIXME retrieves log messages from all instances, the logging queue is not separated
   */
  // bool usvfsGetLogMessages(const char* buffer, size_t size,
  //                                           bool blocking = false);

  /**
   * retrieves a readable representation of the vfs tree
   */
  [[nodiscard]] std::string usvfsCreateVFSDump() const noexcept;

  /**
   * adds an executable to the blacklist so it doesn't get exposed to the virtual
   * file system
   * @param executableName  name of the executable
   */
  void usvfsBlacklistExecutable(const std::string& executableName) noexcept;

  /**
   * clears the executable blacklist
   */
  void usvfsClearExecutableBlacklist() noexcept;

  /**
   * adds a file suffix to a list to skip during file linking
   * .txt and some_file.txt are both valid file suffixes,
   * not to be confused with file extensions
   * @param fileSuffix  a valid file suffix
   */
  void usvfsAddSkipFileSuffix(const std::string& fileSuffix) noexcept;

  /**
   * clears the file suffix skip-list
   */
  void usvfsClearSkipFileSuffixes() noexcept;

  /**
   * adds a directory name that will be skipped during directory linking.
   * Not a path. Any directory matching the name will be skipped,
   * regardless of its path, for example if .git is added,
   * any sub-path or root-path containing a .git directory
   * will have the .git directory skipped during directory linking
   * @param directory  name of the directory
   */
  void usvfsAddSkipDirectory(const std::string& directory) noexcept;

  /**
   * clears the directory skip-list
   */
  void usvfsClearSkipDirectories();

  /**
   * adds a library to be force loaded when the given process is injected
   * @param
   */
  void usvfsForceLoadLibrary(const std::string& processName,
                             const std::string& libraryPath) noexcept;

  /**
   * clears all previous calls to ForceLoadLibrary
   */
  void usvfsClearLibraryForceLoads() noexcept;

  /**
   * print debugging info about the vfs. The format is currently not fixed and may
   * change between usvfs versions
   */
  void usvfsPrintDebugInfo() noexcept;

  void setDebugMode(bool value) noexcept;

  void setProcessDelay(std::chrono::milliseconds processDelay) noexcept;

  // #if defined(UNITTEST) || defined(_WINDLL)
  // void usvfsInitLogging(bool toLocal = false);
  // #endif

  void setLogLevel(LogLevel logLevel) noexcept;

  /**
   * used internally to initialize a process at startup-time as a "slave". Don't call
   * directly
   */
  // DLLEXPORT void cdecl InitHooks(void* userData, size_t userDataSize);

  // the instance and shm names are not updated
  //
  // DLLEXPORT void usvfsUpdateParameters(usvfsParameters* p);

  // DLLEXPORT int usvfsCreateMiniDump(PEXCEPTION_POINTERS exceptionPtrs,
  // CrashDumpsType type,
  // const wchar_t* dumpPath);

  static const char* usvfsVersionString();

  void mount() noexcept(false);
  bool unmount() noexcept;

  bool isMounted() const noexcept;

  void setUpperDir(std::string upperDir) noexcept;

  static bool
  fileNameInSkipSuffixes(const std::string& fileName,
                         const std::set<std::string>& skipSuffixes) noexcept;
  static bool
  fileNameInSkipDirectories(const std::string& directoryName,
                            const std::set<std::string>& skipDirectories) noexcept;

private:
  UsvfsManager() noexcept;

  struct ForcedLibrary
  {
    const std::string processName;
    const std::string libraryPath;
  };

  void run_fuse(std::unique_ptr<MountState> state);
  [[nodiscard]] bool fileNameInSkipSuffixes(const std::string& fileName) const noexcept;
  [[nodiscard]] bool
  fileNameInSkipDirectories(const std::string& directoryName) const noexcept;

  // bool assertPathExists(const std::string& path) const noexcept;
  [[nodiscard]] bool pathExists(const std::string& path) const noexcept;

  [[nodiscard]] std::vector<std::string>
  librariesToForceLoad(const std::string& processName) const noexcept;

  bool anyProcessRunning() const noexcept;

  std::set<std::string> m_skipFileSuffixes;
  std::set<std::string> m_skipDirectories;
  std::set<std::string> m_executableBlacklist;
  std::vector<ForcedLibrary> m_forceLoadLibraries;
  mutable std::shared_mutex m_mtx;
  std::vector<std::unique_ptr<MountState>> m_mounts;
  std::vector<std::unique_ptr<MountState>> m_pendingMounts;
  std::vector<std::unique_ptr<QProcess>> m_spawnedProcesses;
  std::chrono::milliseconds m_processDelay = std::chrono::milliseconds::zero();

  std::string m_upperDir;
  bool m_debugMode = false;
};
