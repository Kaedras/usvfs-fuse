#pragma once

#include "logging.h"
#include <QString>
#include <QStringList>
#include <memory>
#include <optional>
#include <set>
#include <shared_mutex>
#include <string>
#include <vector>

// forward declarations
struct MountState;

namespace linkFlag
{
static constexpr unsigned int CREATE_TARGET =
    0x00000004;  // if set, file creation (including move or copy) operations to
// destination will be redirected to the source. Only one createtarget
// can be set for a destination folder so this flag will replace
// the previous create target.
// If there different create-target have been set for an element and
// one of its ancestors, the inner-most create-target is used
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
   * @brief remove all virtual mappings
   */
  void usvfsClearVirtualMappings() noexcept;

  /**
   * @brief link a file virtually
   * @throw std::runtime_error
   * @note: the directory the destination file resides in has to exist - at least
   * virtually.
   */
  void usvfsVirtualLinkFile(const std::string& source,
                            const std::string& destination) noexcept(false);

  /**
   * @brief link a directory virtually
   * @throw std::runtime_error
   */
  void usvfsVirtualLinkDirectoryStatic(const std::string& source,
                                       const std::string& destination,
                                       unsigned int flags = 0) noexcept(false);

  /**
   * @brief retrieve a list of all processes connected to the vfs
   */
  const std::vector<pid_t>& usvfsGetVFSProcessList() const noexcept;

  /**
   * @brief spawn a new process that can see the virtual file system.
   */
  pid_t usvfsCreateProcessHooked(const std::string& file,
                                 const std::string& arg) noexcept;

  /**
   * @brief spawn a new process that can see the virtual file system.
   * @param env Environment variables to add to the process. Strings should be formatted
   * as `KEY=VALUE`.
   */
  pid_t usvfsCreateProcessHooked(const std::string& file, const std::string& arg,
                                 const std::string& workDir,
                                 const std::vector<std::string>& env = {}) noexcept;

  /**
   * @brief spawn a new process that can see the virtual file system.
   */
  pid_t usvfsCreateProcessHooked(const QString& file, const QString& arg) noexcept;

  /**
   * @brief spawn a new process that can see the virtual file system.
   * @param env Environment variables to add to the process
   */
  pid_t usvfsCreateProcessHooked(const QString& file, const QString& arg,
                                 const QString& workDir,
                                 const QStringList& env = {}) noexcept;

  /**
   * retrieve a single log message.
   * FIXME There is currently no way to unblock from the caller side
   * FIXME retrieves log messages from all instances, the logging queue is not separated
   */
  // bool usvfsGetLogMessages(const char* buffer, size_t size,
  //                                           bool blocking = false);

  /**
   * @brief retrieve a readable representation of the vfs tree
   */
  [[nodiscard]] std::string usvfsCreateVFSDump() const noexcept;

  /**
   * @brief adds an executable to the blacklist, so it doesn't get exposed to the
   * virtual file system
   * @param executableName  name of the executable
   */
  void usvfsBlacklistExecutable(const std::string& executableName) noexcept;

  /**
   * @brief clear the executable blacklist
   */
  void usvfsClearExecutableBlacklist() noexcept;

  /**
   * @brief add a file suffix to a list to skip during file linking
   * .txt and some_file.txt are both valid file suffixes,
   * not to be confused with file extensions
   * @param fileSuffix  a valid file suffix
   */
  void usvfsAddSkipFileSuffix(const std::string& fileSuffix) noexcept;

  /**
   * @brief clear the file suffix skip-list
   */
  void usvfsClearSkipFileSuffixes() noexcept;

  /**
   * add a directory name that will be skipped during directory linking.
   * Not a path. Any directory matching the name will be skipped,
   * regardless of its path, for example if .git is added,
   * any sub-path or root-path containing a .git directory
   * will have the .git directory skipped during directory linking
   * @param directory  name of the directory
   */
  void usvfsAddSkipDirectory(const std::string& directory) noexcept;

  /**
   * @brief clear the directory skip-list
   */
  void usvfsClearSkipDirectories() noexcept;

  /**
   * @brief add a library to be force loaded when the given process is injected
   * @param
   */
  void usvfsForceLoadLibrary(const std::string& processName,
                             const std::string& libraryPath) noexcept;

  /**
   * @brief clear all previous calls to ForceLoadLibrary
   */
  void usvfsClearLibraryForceLoads() noexcept;

  /**
   * @brief print debugging info about the vfs. The format is currently not fixed and
   * may change between usvfs versions
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

  // set whether to create mounts in a new user mount namespace
  void setUseMountNamespace(bool value) noexcept;

  // set whether wine is used. Required to use WINEDLLOVERRIDES
  void setWine(bool value) noexcept;

  static bool
  fileNameInSkipSuffixes(std::string_view fileName,
                         const std::set<std::string>& skipSuffixes) noexcept;
  static bool
  fileNameInSkipDirectories(std::string_view directoryName,
                            const std::set<std::string>& skipDirectories) noexcept;

  /**
   * @brief Reset everything to default values and call unmount()
   */
  void reset() noexcept;

private:
  UsvfsManager() noexcept;

  struct ForcedLibrary
  {
    const std::string processName;
    const std::string libraryPath;
  };

  void run_fuse(std::unique_ptr<MountState> state) noexcept;
  [[nodiscard]] bool fileNameInSkipSuffixes(std::string_view fileName) const noexcept;
  [[nodiscard]] bool
  fileNameInSkipDirectories(std::string_view directoryName) const noexcept;

  [[nodiscard]] std::vector<std::string>
  librariesToForceLoad(std::string_view processName) const noexcept;

  /**
   * @brief check if any process in m_spawnedProcesses is still running
   */
  bool anyProcessRunning() const noexcept;

  // mount function without locking for internal use
  bool mountInternal() noexcept;
  // unmount function without locking for internal use
  bool unmountInternal() noexcept;

  bool m_debugMode                         = false;
  bool m_useMountNamespace                 = false;
  bool m_wine                              = false;
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
  LogLevel m_logLevel = LogLevel::Debug;
};
