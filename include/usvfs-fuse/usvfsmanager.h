#pragma once

#include "logging.h"
#include <shared_mutex>
#include <string>

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
  template <typename Mutex>
  class rotating_file_sink;
}  // namespace sinks
}  // namespace spdlog

struct fuse_file_info;
struct fuse_operations;
struct fuse;

namespace linkFlag
{
static constexpr unsigned int CREATE_TARGET =
    0x00000004;  // if set, file creation (including move or copy) operations to
// destination will be redirected to the source. Only one createtarget
// can be set for a destination folder so this flag will replace
// the previous create target.
// If there different create-target have been set for an element and
// one of its ancestors, the inner-most create-target is used
static constexpr unsigned int RECURSIVE =
    0x00000008;  // if set, directories are linked recursively
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
  bool usvfsVirtualLinkFile(const std::string& source, const std::string& destination,
                            unsigned int flags) noexcept;

  /**
   * link a directory virtually. This static variant recursively links all files
   * individually, change notifications are used to update the information.
   * (virtually or physically)
   */
  bool usvfsVirtualLinkDirectoryStatic(const std::string& source,
                                       const std::string& destination,
                                       unsigned int flags) noexcept;

  /**
   * retrieve a list of all processes connected to the vfs
   */
  const std::vector<pid_t>& usvfsGetVFSProcessList() const noexcept;

  /**
   * @brief spawn a new process that can see the virtual file system.
   * @param env Environment variables to add to the process
   */
  pid_t usvfsCreateProcessHooked(
      const std::string& file, const std::string& arg, const std::string& workDir,
      std::optional<std::reference_wrapper<std::vector<std::string>>> env =
          std::nullopt) noexcept;

  pid_t usvfsCreateProcessHooked(const std::string& file,
                                 const std::string& arg) noexcept;

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
  void usvfsClearSkipDirectories() noexcept;

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

  void setLogLevel(LogLevel logLevel) noexcept;

  void setLogFile(const std::string& logFile) noexcept;

  // DLLEXPORT int usvfsCreateMiniDump(PEXCEPTION_POINTERS exceptionPtrs,
  // CrashDumpsType type,
  // const wchar_t* dumpPath);

  static const char* usvfsVersionString() noexcept;

  bool mount() noexcept;
  bool unmount() noexcept;

  bool isMounted() const noexcept;

  void setUpperDir(std::string upperDir) noexcept;

  // set whether to create mounts in a new user mount namespace
  void setUseMountNamespace(bool value) noexcept;

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

  [[nodiscard]] std::vector<std::string>
  librariesToForceLoad(const std::string& processName) const noexcept;

  bool anyProcessRunning() const noexcept;

  // mount function without locking for internal use
  bool mountInternal() noexcept;

  bool m_debugMode         = false;
  bool m_useMountNamespace = false;
  std::string m_upperDir;
  std::chrono::milliseconds m_processDelay = std::chrono::milliseconds::zero();
  std::set<std::string> m_skipFileSuffixes;
  std::set<std::string> m_skipDirectories;
  std::set<std::string> m_executableBlacklist;
  std::vector<ForcedLibrary> m_forceLoadLibraries;

  mutable std::shared_mutex m_mtx;
  pid_t m_nsPidFd = -1;  // file descriptor to access the mount namespace
  std::vector<std::unique_ptr<MountState>> m_mounts;
  std::vector<std::unique_ptr<MountState>> m_pendingMounts;
  std::vector<pid_t> m_spawnedProcesses;
  std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> m_fileSink;
};
