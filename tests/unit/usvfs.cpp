#include "../../src/utils.h"

#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <ranges>
#include <source_location>
#include <sys/statvfs.h>

#include "usvfs-fuse/usvfsmanager.h"

using namespace std;
namespace fs = std::filesystem;

#define INIT_SCOPED_TRACE()                                                            \
  SCOPED_TRACE(testing::Message() << "called from " << location.file_name() << ':'     \
                                  << location.line() << ", path: " << path)

namespace
{

constexpr mode_t mode          = 0755;
LogLevel logLevel              = LogLevel::Trace;
constexpr bool enableDebugMode = true;

const fs::path base  = fs::temp_directory_path() / "usvfs";
const fs::path src   = base / "src";
const fs::path mnt   = base / "mnt";
const fs::path mnt2  = base / "mnt2";
const fs::path upper = base / "upper";

const vector filesToCheck{
    pair{mnt / "a.txt", "test a"},
    pair{mnt / "a/a.txt", "test a/a"},
    pair{mnt / "b.txt", "test b"},
    pair{mnt2 / "c.txt", "test c"},
    pair{mnt / "already_existed.txt", "test already_existed"},
    pair{mnt / "already_existing_dir/already_existed0.txt",
         "test already_existing_dir/already_existed0"},
};
const vector filesToCheckCaseInsensitive{
    pair{mnt / "A.txt", "test a"},
    pair{mnt / "A/A.txt", "test a/a"},
    pair{mnt / "B.txt", "test b"},
    pair{mnt2 / "C.txt", "test c"},
    pair{mnt / "ALREADY_EXISTED.txt", "test already_existed"},
    pair{mnt / "ALREADY_EXISTING_DIR/ALREADY_EXISTED0.txt",
         "test already_existing_dir/already_existed0"},
};

const vector filesToCreate{pair{src / "a/a.txt", "test a"},
                           pair{src / "a/a/a.txt", "test a/a"},
                           pair{src / "b/b.txt", "test b"},
                           pair{src / "c/c.txt", "test c"},
                           pair{mnt / "already_existed.txt", "test already_existed"},
                           pair{mnt / "already_existing_dir/already_existed0.txt",
                                "test already_existing_dir/already_existed0"},
                           pair{mnt / "A.txt", "TEST A"}};

const vector srcDirsToCreate{src / "a",           src / "b",
                             src / "c",           src / "a/a/",
                             src / "a/empty_dir", mnt / "already_existing_dir"};

bool createTmpDirs()
{
  error_code ec;
  for (const auto& srcDir : srcDirsToCreate) {
    fs::create_directories(srcDir, ec);
    if (ec) {
      cerr << "cannot create source dir: " << ec.message() << "\n";
      return false;
    }
  }

  fs::create_directories(mnt, ec);
  if (ec) {
    cerr << "cannot create mount dir: " << ec.message() << "\n";
    fs::remove_all(base);
    return false;
  }
  fs::create_directories(mnt2, ec);
  if (ec) {
    cerr << "cannot create mount2 dir: " << ec.message() << "\n";
    fs::remove_all(base);
    return false;
  }

  fs::create_directories(upper, ec);
  if (ec) {
    cerr << "cannot create upper dir: " << ec.message() << "\n";
    fs::remove_all(base);
    return false;
  }

  for (const auto& [file, content] : filesToCreate) {
    ofstream ofs(file);
    ofs << content;
  }

  return true;
}

bool cleanup()
{
  UsvfsManager::instance()->reset();
  std::error_code ec;
  fs::remove_all(base, ec);
  if (ec) {
    std::cerr << "cannot remove test dir: " << ec.message() << "\n";
    return false;
  }
  return true;
}

bool runCmd(const string& cmd)
{
  cout << "running " << quoted(cmd) << endl;
  const int result = system(cmd.c_str());
  return WIFEXITED(result) && WEXITSTATUS(result) == 0;
}

void initLogging()
{
  auto usvfs = UsvfsManager::instance();
  usvfs->setLogLevel(logLevel);
  usvfs->setLogFile("/tmp/usvfs.log");
}

void dumpUsvfs()
{
  const auto usvfs = UsvfsManager::instance();
  const auto dump  = usvfs->usvfsCreateVFSDump();
  cout << "=============== usvfs dump ===============\n"
       << dump << "==========================================" << endl;
}

void openFile(const fs::path& path,
              source_location location = source_location::current())
{
  INIT_SCOPED_TRACE();
  int fd;
  ASSERT_NE(fd = open(path.c_str(), O_RDONLY), -1)
      << "error opening file " << path << ": " << strerror(errno);
  EXPECT_EQ(close(fd), 0) << "error closing file " << path << ": " << strerror(errno);
}

void openFileWithFailure(const fs::path& path, int error,
                         source_location location = source_location::current())
{
  INIT_SCOPED_TRACE();
  int fd;
  EXPECT_EQ(fd = open(path.c_str(), O_RDONLY), -1)
      << "error opening file " << path << ": " << strerror(errno);
  EXPECT_EQ(errno, error) << "expected " << strerrorname_np(error) << ", got "
                          << strerrorname_np(errno);
  if (fd != -1) {
    EXPECT_EQ(close(fd), 0) << "error closing file '" << path
                            << "': " << strerror(errno);
  }
}

void checkFileContent(const fs::path& path, const string& expectedContent,
                      source_location location = source_location::current())
{
  INIT_SCOPED_TRACE();
  int fd;
  ASSERT_NE(fd = open(path.c_str(), O_RDONLY), -1)
      << "error opening file '" << path << "': " << strerror(errno);

  array<char, 4096> buf{};
  const ssize_t readBytes = read(fd, buf.data(), buf.size());
  int closeResult         = close(fd);
  EXPECT_NE(readBytes, -1) << "read error in file " << path << ": " << strerror(errno);

  EXPECT_EQ(closeResult, 0) << "error closing file " << path << ": " << strerror(errno);

  EXPECT_EQ(string(buf.data(), readBytes), expectedContent);
}

void statFile(const fs::path& path,
              source_location location = source_location::current())
{
  INIT_SCOPED_TRACE();
  struct stat st{};
  EXPECT_EQ(stat(path.c_str(), &st), 0)
      << "stat failed for " << path << ": " << strerror(errno);
  if (stat(path.c_str(), &st) == 0) {
    EXPECT_TRUE(S_ISREG(st.st_mode)) << path << " is not a regular file";
  }
}

void statDirectory(const fs::path& path,
                   source_location location = source_location::current())
{
  INIT_SCOPED_TRACE();
  struct stat st{};
  EXPECT_EQ(stat(path.c_str(), &st), 0)
      << "stat failed for " << path << ": " << strerror(errno);
  if (stat(path.c_str(), &st) == 0) {
    EXPECT_TRUE(S_ISDIR(st.st_mode)) << path << " is not a regular file";
  }
}

void createDir(const string& path,
               source_location location = source_location::current())
{
  INIT_SCOPED_TRACE();
  EXPECT_EQ(mkdir(path.c_str(), mode), 0)
      << "error creating " << path << ": " << strerror(errno);
}

void createDirWithFailure(const string& path, int error,
                          source_location location = source_location::current())
{
  INIT_SCOPED_TRACE();
  EXPECT_EQ(mkdir(path.c_str(), mode), -1);
  EXPECT_EQ(errno, error) << "expected " << strerrorname_np(error) << ", got "
                          << strerrorname_np(errno);
}

void unlinkFile(const string& path,
                source_location location = source_location::current())
{
  INIT_SCOPED_TRACE();
  ASSERT_EQ(unlink(path.c_str()), 0)
      << "error unlinking file '" << path << "': " << strerror(errno);

  openFileWithFailure(path, ENOENT);
}

void unlinkFileWithFailure(const string& path, int error,
                           source_location location = source_location::current())
{
  INIT_SCOPED_TRACE();
  EXPECT_EQ(unlink(path.c_str()), -1) << "error: " << strerror(errno);
  EXPECT_EQ(errno, error) << "expected " << strerrorname_np(error) << ", got "
                          << strerrorname_np(errno);
}

void unlinkDir(const string& path,
               source_location location = source_location::current())
{
  INIT_SCOPED_TRACE();
  ASSERT_EQ(rmdir(path.c_str()), 0) << "error: " << strerror(errno);

  openFileWithFailure(path, ENOENT);
}

void unlinkDirWithFailure(const string& path, int error,
                          source_location location = source_location::current())
{
  INIT_SCOPED_TRACE();
  EXPECT_EQ(rmdir(path.c_str()), -1)
      << "error for '" << path << "': " << strerror(errno);
  EXPECT_EQ(errno, error) << "expected " << strerrorname_np(error) << ", got "
                          << strerrorname_np(errno);
}

bool createFile(const string& path, const string& content,
                source_location location = source_location::current())
{
  INIT_SCOPED_TRACE();
  try {
    ofstream ofs(path);
    ofs.exceptions(std::ios_base::failbit | std::ios_base::badbit);
    ofs << content;
    ofs.close();
    return true;
  } catch (const exception& ex) {
    cerr << "error creating file '" << path << "': " << ex.what()
         << ", errno: " << strerrorname_np(errno) << '\n';
    return false;
  }
}

}  // namespace

class UsvfsTest : public testing::Test
{
protected:
  UsvfsTest() = default;
  void SetUp() override
  {
    initLogging();
    ASSERT_TRUE(createTmpDirs());
    runCmd("tree "s + base.string());
    const auto usvfs = UsvfsManager::instance();
    usvfs->setDebugMode(enableDebugMode);

    ASSERT_NO_THROW(
        usvfs->usvfsVirtualLinkDirectoryStatic((src / "a").string(), mnt.string()));
    ASSERT_NO_THROW(
        usvfs->usvfsVirtualLinkDirectoryStatic((src / "b").string(), mnt.string()));
    ASSERT_NO_THROW(usvfs->usvfsVirtualLinkDirectoryStatic(upper, mnt.string(),
                                                           linkFlag::CREATE_TARGET));

    ASSERT_NO_THROW(usvfs->usvfsVirtualLinkFile(src / "c/c.txt", mnt2 / "c.txt"));

    ASSERT_NO_THROW(usvfs->mount());
    dumpUsvfs();
  }
  void TearDown() override
  {
    const auto usvfs = UsvfsManager::instance();
    dumpUsvfs();
    ASSERT_TRUE(usvfs->unmount());
    runCmd("tree "s + base.string());
    ASSERT_TRUE(cleanup());
  }
};

TEST_F(UsvfsTest, CanMount)
{
  ASSERT_TRUE(UsvfsManager::instance()->isMounted());
}

TEST_F(UsvfsTest, getattr)
{
  statFile(mnt / "a.txt");
  statFile(mnt / "b.txt");
  statFile(mnt2 / "c.txt");
  statDirectory(mnt / "a");
  statFile(mnt / "a/a.txt");
  statDirectory(mnt / "empty_dir");

  statFile(mnt / "already_existed.txt");
  statDirectory(mnt / "already_existing_dir");
  statFile(mnt / "already_existing_dir/already_existed0.txt");

  openFileWithFailure(mnt / "DOES_NOT_EXIST", ENOENT);
}

TEST_F(UsvfsTest, getattrCaseInsensitive)
{
  statFile(mnt / "A.tXt");
  statFile(mnt / "B.tXT");
  statFile(mnt2 / "C.txT");
  statDirectory(mnt / "A");
  statFile(mnt / "A/A.tXT");
  statDirectory(mnt / "EmpTy_dIr");

  statFile(mnt / "alreADy_exiSteD.txt");
  statDirectory(mnt / "aLreAdy_EXisTing_diR");
  statFile(mnt / "ALREADY_EXISTING_DIR/ALREADY_EXISTED0.txt");

  openFileWithFailure(mnt / "DOES_NOT_EXIST", ENOENT);
}

TEST_F(UsvfsTest, open)
{
  for (const auto& file : filesToCheck | views::keys) {
    openFile(file);
  }

  openFileWithFailure(mnt / "DOES_NOT_EXIST", ENOENT);
}

TEST_F(UsvfsTest, openCaseInsensitive)
{
  for (const auto& file : filesToCheckCaseInsensitive | views::keys) {
    openFile(file);
  }

  openFileWithFailure(mnt / "DOES_NOT_EXIST", ENOENT);
}

TEST_F(UsvfsTest, readdir)
{
  EXPECT_TRUE(runCmd("tree "s + mnt.c_str())) << "error: " << strerror(errno);
}

TEST_F(UsvfsTest, mkdir)
{
  createDir(mnt / "new_dir");
  createDir(mnt / "new_dir/b");
  createDir(mnt / "new_dir/c");

  createDirWithFailure(mnt / "a", EEXIST);
  createDirWithFailure(mnt / "b/c/d/e", ENOENT);
}

TEST_F(UsvfsTest, mkdirCaseInsensitive)
{
  createDir(mnt / "new_dir");
  createDir(mnt / "NEW_DIR/b");
  createDir(mnt / "NEW_DIR/c");
  createDir(mnt / "A/new_dir");
  createDir(mnt / "empty_DIR/new_dir");

  createDirWithFailure(mnt / "A", EEXIST);
  createDirWithFailure(mnt / "b/c/d/e", ENOENT);
}

TEST_F(UsvfsTest, read)
{
  for (const auto& [filePath, content] : filesToCheck) {
    checkFileContent(filePath, content);
  }
}

TEST_F(UsvfsTest, readCaseInsensitive)
{
  for (const auto& [filePath, content] : filesToCheckCaseInsensitive) {
    checkFileContent(filePath, content);
  }
}

TEST_F(UsvfsTest, write)
{
  char buffer[256];
  memset(buffer, 'A', 256);
  for (const auto& filePath : filesToCheck | views::keys) {
    int fd = open(filePath.c_str(), O_WRONLY | O_TRUNC);
    ASSERT_NE(fd, -1) << "error opening " << filePath << ": " << strerror(errno);
    ssize_t size = write(fd, buffer, 256);
    EXPECT_EQ(size, 256) << "error writing to " << filePath << ": " << strerror(errno);
    close(fd);
  }
}

TEST_F(UsvfsTest, writeCaseInsensitive)
{
  char buffer[256];
  memset(buffer, 'A', 256);
  for (const auto& filePath : filesToCheckCaseInsensitive | views::keys) {
    int fd = open(filePath.c_str(), O_WRONLY | O_TRUNC);
    ASSERT_NE(fd, -1) << "error opening " << filePath << ": " << strerror(errno);
    ssize_t size = write(fd, buffer, 256);
    EXPECT_EQ(size, 256) << "error writing to " << filePath << ": " << strerror(errno);
    close(fd);
  }
}

TEST_F(UsvfsTest, unlink)
{
  unlinkFile(mnt / "a.txt");
  unlinkFile(mnt / "already_existed.txt");

  unlinkDir(mnt / "empty_dir");

  unlinkFileWithFailure(mnt / "a", EISDIR);
  unlinkDirWithFailure(mnt / "a", ENOTEMPTY);

  EXPECT_TRUE(runCmd("rm -rf "s + mnt.c_str() + "/a"));
}

TEST_F(UsvfsTest, unlinkCaseInsensitive)
{
  unlinkFile(mnt / "A.tXT");
  unlinkFile(mnt / "already_EXISTED.txt");

  unlinkDir(mnt / "emPTY_dir");

  unlinkFileWithFailure(mnt / "A", EISDIR);
  unlinkDirWithFailure(mnt / "A", ENOTEMPTY);

  EXPECT_TRUE(runCmd("rm -rf "s + mnt.c_str() + "/A"));
}

TEST_F(UsvfsTest, rename)
{
  EXPECT_EQ(rename((mnt / "a.txt").c_str(), (mnt / "asdf.txt").c_str()), 0)
      << "error: " << strerror(errno);

  checkFileContent(mnt / "asdf.txt", "test a");

  // opening the original file should fail with ENOENT
  openFileWithFailure(mnt / "a.txt", ENOENT);
}

TEST_F(UsvfsTest, renameCaseInsensitive)
{
  EXPECT_EQ(rename((mnt / "A.txt").c_str(), (mnt / "ASDF.txt").c_str()), 0)
      << "error: " << strerror(errno);

  checkFileContent(mnt / "asdf.TXT", "test a");

  // opening the original file should fail with ENOENT
  openFileWithFailure(mnt / "A.txT", ENOENT);
}

TEST_F(UsvfsTest, chmod)
{
  fs::path file = mnt / "a.txt";
  struct stat st{};

  // get old mode
  ASSERT_EQ(stat(file.c_str(), &st), 0) << "error: " << strerror(errno);
  mode_t oldMode = st.st_mode;

  // set mode to 0751
  chmod(file.c_str(), 0751);

  // get new mode
  ASSERT_EQ(stat(file.c_str(), &st), 0) << "error: " << strerror(errno);
  mode_t newMode = st.st_mode;

  EXPECT_EQ(newMode & 0777, 0751);

  // compare modes
  EXPECT_NE(newMode, oldMode);
}

TEST_F(UsvfsTest, chmodCaseInsensitive)
{
  fs::path file = mnt / "A.TXT";
  struct stat st{};

  // get old mode
  ASSERT_EQ(stat(file.c_str(), &st), 0) << "error: " << strerror(errno);
  mode_t oldMode = st.st_mode;

  // set mode to 0751
  chmod(file.c_str(), 0751);

  // get new mode
  ASSERT_EQ(stat(file.c_str(), &st), 0) << "error: " << strerror(errno);
  mode_t newMode = st.st_mode;

  EXPECT_EQ(newMode & 0777, 0751);

  // compare modes
  EXPECT_NE(newMode, oldMode);
}

TEST_F(UsvfsTest, create)
{
  static constexpr int oflags = O_WRONLY | O_CREAT | O_EXCL;

  auto createFile = [](const string& path) {
    int fd;
    ASSERT_GT(fd = open(path.c_str(), oflags, mode), -1)
        << "opening " << path << " failed: " << strerror(errno);
    EXPECT_EQ(close(fd), 0);

    EXPECT_EQ(open(path.c_str(), oflags, mode), -1);
    EXPECT_EQ(errno, EEXIST) << "expected EEXIST, got " << strerrorname_np(errno);
  };

  createFile(mnt / "new_file.txt");
  createFile(mnt / "a/new_file.txt");

  ASSERT_EQ(mkdir((mnt / "new_dir").c_str(), mode), 0) << "error: " << strerror(errno);
  int fd;
  ASSERT_GT(fd = open((mnt / "new_dir/testfile.txt").c_str(), oflags, mode), -1)
      << "error: " << strerror(errno);
  EXPECT_EQ(close(fd), 0);
}

TEST_F(UsvfsTest, createCaseInsensitive)
{
  static constexpr int oflags = O_WRONLY | O_CREAT | O_EXCL;

  auto createFile = [](const string& path, const string& pathCI) {
    int fd;
    ASSERT_GT(fd = open(path.c_str(), oflags, mode), -1)
        << "opening " << path << " failed: " << strerror(errno);
    EXPECT_EQ(close(fd), 0);

    EXPECT_EQ(open(pathCI.c_str(), oflags, mode), -1);
    EXPECT_EQ(errno, EEXIST) << "expected EEXIST, got " << strerrorname_np(errno);
  };

  createFile(mnt / "new_file.txt", mnt / "NEW_FILE.TXT");
  createFile(mnt / "A/new_file.txt", mnt / "A/NEW_FILE.TXT");

  ASSERT_EQ(mkdir((mnt / "NEW_DIR").c_str(), mode), 0) << "error: " << strerror(errno);
  int fd;
  ASSERT_GT(fd = open((mnt / "new_dir/testfile.txt").c_str(), oflags, mode), -1)
      << "error: " << strerror(errno);
  EXPECT_EQ(close(fd), 0);
}

TEST_F(UsvfsTest, recreateFile)
{
  static constexpr int oflags = O_WRONLY | O_CREAT | O_EXCL;

  ASSERT_EQ(remove((mnt / "a.txt").c_str()), 0) << "error: " << strerror(errno);
  int fd;
  ASSERT_GT(fd = open((mnt / "A.txt").c_str(), oflags, mode), 0)
      << "error: " << strerror(errno);
  EXPECT_EQ(close(fd), 0);

  ASSERT_GT(fd = open((src / "a/a.txt").c_str(), O_RDONLY, mode), 0)
      << "error: " << strerror(errno);
  EXPECT_EQ(close(fd), 0);
}

TEST_F(UsvfsTest, statfs)
{
  struct statvfs buf;
  EXPECT_GT(statvfs(mnt.c_str(), &buf), -1) << "error: " << strerror(errno);
}

TEST_F(UsvfsTest, createMissingParentDirInUpperDir)
{
  int fd;
  EXPECT_GT(fd = open((mnt / "empty_dir/test").c_str(), O_CREAT | O_EXCL, mode), -1)
      << "error: " << strerror(errno);
  EXPECT_EQ(close(fd), 0);
}

TEST(Usvfs, MergeModDirectories)
{
  initLogging();

  ASSERT_TRUE(fs::create_directories(src / "mod0"));
  ASSERT_TRUE(fs::create_directories(src / "mod0/data"));
  ASSERT_TRUE(fs::create_directories(src / "mod1"));
  ASSERT_TRUE(fs::create_directories(src / "mod1/DATA"));
  ASSERT_TRUE(fs::create_directories(src / "mod2"));
  ASSERT_TRUE(fs::create_directories(src / "mod3"));
  ASSERT_TRUE(fs::create_directories(src / "mod4"));
  ASSERT_TRUE(fs::create_directories(src / "profile"));
  ASSERT_TRUE(fs::create_directories(src / "overwrite"));

  ASSERT_TRUE(fs::create_directories(mnt));
  ASSERT_TRUE(fs::create_directories(mnt2));

  ASSERT_TRUE(createFile(src / "mod0/a.txt", "mod0 a"));
  ASSERT_TRUE(createFile(src / "mod1/A.txt", "mod1 a"));

  ASSERT_TRUE(createFile(src / "profile/Plugins.txt", "mod0.esp\nmod1.esp"));
  ASSERT_TRUE(createFile(mnt2 / "plugins.txt", ""));

  auto usvfs = UsvfsManager::instance();
  usvfs->setProcessDelay(10ms);
  ASSERT_NO_THROW(
      usvfs->usvfsVirtualLinkDirectoryStatic((src / "mod0").string(), mnt.string()));
  ASSERT_NO_THROW(
      usvfs->usvfsVirtualLinkDirectoryStatic((src / "mod1").string(), mnt.string()));
  ASSERT_NO_THROW(
      usvfs->usvfsVirtualLinkDirectoryStatic((src / "mod2").string(), mnt.string()));
  ASSERT_NO_THROW(
      usvfs->usvfsVirtualLinkDirectoryStatic((src / "mod3").string(), mnt.string()));
  ASSERT_NO_THROW(
      usvfs->usvfsVirtualLinkDirectoryStatic((src / "mod4").string(), mnt.string()));
  ASSERT_NO_THROW(usvfs->usvfsVirtualLinkDirectoryStatic(
      (src / "overwrite").string(), mnt.string(), linkFlag::CREATE_TARGET));

  ASSERT_NO_THROW(usvfs->usvfsVirtualLinkFile((src / "profile/Plugins.txt").string(),
                                              (mnt2 / "plugins.txt").string()));

  ASSERT_TRUE(usvfs->mount());

  cout << usvfs->usvfsCreateVFSDump();

  checkFileContent(mnt / "a.txt", "mod1 a");
  checkFileContent(mnt / "A.txt", "mod1 a");
  ASSERT_TRUE(createFile(mnt / "daTA/TEST", "test"));
  checkFileContent(mnt / "data/TEst", "test");

  checkFileContent(mnt2 / "plugins.txt", "mod0.esp\nmod1.esp");
  checkFileContent(mnt2 / "Plugins.txt", "mod0.esp\nmod1.esp");

  EXPECT_TRUE(runCmd("tree "s + mnt.string()));
  EXPECT_TRUE(runCmd("tree "s + mnt2.string()));
  EXPECT_TRUE(usvfs->unmount());
  EXPECT_TRUE(cleanup());
}

TEST(Usvfs, CreateProcessHooked)
{
  initLogging();
  ASSERT_TRUE(createTmpDirs());

  auto usvfs = UsvfsManager::instance();
  usvfs->setProcessDelay(10ms);

  ASSERT_NO_THROW(
      usvfs->usvfsVirtualLinkDirectoryStatic((src / "a").string(), mnt.string()));
  ASSERT_NO_THROW(
      usvfs->usvfsVirtualLinkDirectoryStatic((src / "b").string(), mnt.string()));
  ASSERT_NO_THROW(usvfs->usvfsVirtualLinkFile(src / "c/c.txt", mnt / "c.txt"));

  pid_t pid = usvfs->usvfsCreateProcessHooked("tree", ".", mnt.string());
  ASSERT_GE(pid, 0);

  int status;
  EXPECT_GE(waitpid(pid, &status, 0), 0) << "error: " << strerror(errno);
  EXPECT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 0);
  usvfs->unmount();

  this_thread::sleep_for(10ms);
  EXPECT_TRUE(cleanup());
}

TEST(Usvfs, CreateProcessHooked_WithMountNamespace)
{
  initLogging();
  ASSERT_TRUE(createTmpDirs());

  auto usvfs = UsvfsManager::instance();
  usvfs->setProcessDelay(10ms);
  usvfs->setUseMountNamespace(true);

  ASSERT_NO_THROW(
      usvfs->usvfsVirtualLinkDirectoryStatic((src / "a").string(), mnt.string(), 0));
  ASSERT_NO_THROW(
      usvfs->usvfsVirtualLinkDirectoryStatic((src / "b").string(), mnt.string(), 0));
  ASSERT_NO_THROW(usvfs->usvfsVirtualLinkFile(src / "c/c.txt", mnt / "c.txt"));

  pid_t pid = usvfs->usvfsCreateProcessHooked("tree", ".", mnt.string());
  ASSERT_GE(pid, 0);

  int status;
  EXPECT_GE(waitpid(pid, &status, 0), 0) << "error: " << strerror(errno);
  EXPECT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 0);

  EXPECT_TRUE(cleanup());
}

TEST(Usvfs, linkDirectoryMountpointCaseInsensitivity)
{
  initLogging();
  ASSERT_TRUE(createTmpDirs());

  auto usvfs = UsvfsManager::instance();

  ASSERT_NO_THROW(usvfs->usvfsVirtualLinkDirectoryStatic(
      (src / "a").string(), mnt.parent_path() / toUpper(mnt.filename().string()), 0));
  ASSERT_TRUE(usvfs->mount());
  ASSERT_TRUE(usvfs->unmount());

  EXPECT_TRUE(cleanup());
}

TEST(Usvfs, linkFileMountpointCaseInsensitivity)
{
  initLogging();
  ASSERT_TRUE(createTmpDirs());

  auto usvfs = UsvfsManager::instance();

  ASSERT_NO_THROW(usvfs->usvfsVirtualLinkFile(
      (src / "a/a.txt").string(),
      mnt.parent_path() / toUpper(mnt.filename().string()) / "a.txt"));
  ASSERT_TRUE(usvfs->mount());
  ASSERT_TRUE(usvfs->unmount());

  EXPECT_TRUE(cleanup());
}

TEST(Usvfs, LinkDirectoryIntoNestedVirtualDirectory_SourceParentLinkedFirst)
{
  initLogging();
  ASSERT_TRUE(createTmpDirs());

  auto usvfs = UsvfsManager::instance();

  ASSERT_NO_THROW(usvfs->usvfsVirtualLinkDirectoryStatic((src / "a").string(), mnt, 0));
  ASSERT_NO_THROW(usvfs->usvfsVirtualLinkDirectoryStatic((src / "b").string(),
                                                         mnt / "empty_dir", 0));
  ASSERT_TRUE(usvfs->mount());
  ASSERT_TRUE(usvfs->unmount());

  EXPECT_TRUE(cleanup());
}

TEST(Usvfs, LinkDirectoryIntoNestedVirtualDirectory_NestedLinkRegisteredFirst)
{
  initLogging();
  ASSERT_TRUE(createTmpDirs());

  auto usvfs = UsvfsManager::instance();

  ASSERT_NO_THROW(usvfs->usvfsVirtualLinkDirectoryStatic((src / "b").string(),
                                                         mnt / "empty_dir", 0));
  ASSERT_NO_THROW(usvfs->usvfsVirtualLinkDirectoryStatic((src / "a").string(), mnt, 0));
  ASSERT_TRUE(usvfs->mount());
  ASSERT_TRUE(usvfs->unmount());

  EXPECT_TRUE(cleanup());
}

TEST(Usvfs, LinkFileIntoNestedVirtualDirectory_SourceParentLinkedFirst)
{
  initLogging();
  ASSERT_TRUE(createTmpDirs());

  auto usvfs = UsvfsManager::instance();

  ASSERT_NO_THROW(usvfs->usvfsVirtualLinkDirectoryStatic((src / "a").string(), mnt, 0));
  ASSERT_NO_THROW(
      usvfs->usvfsVirtualLinkFile((src / "b/b.txt").string(), mnt / "b.txt"));
  ASSERT_TRUE(usvfs->mount());

  checkFileContent(mnt / "a.txt", "test a");
  checkFileContent(mnt / "a/a.txt", "test a/a");
  checkFileContent(mnt / "already_existed.txt", "test already_existed");
  checkFileContent(mnt / "already_existing_dir/already_existed0.txt",
                   "test already_existing_dir/already_existed0");
  checkFileContent(mnt / "b.txt", "test b");

  statDirectory(mnt / "empty_dir");

  EXPECT_TRUE(usvfs->unmount());

  EXPECT_TRUE(cleanup());
}

TEST(Usvfs, LinkFileIntoNestedVirtualDirectory_NestedLinkRegisteredFirst)
{
  initLogging();
  ASSERT_TRUE(createTmpDirs());

  fs::create_directories(mnt / "empty_dir");

  auto usvfs = UsvfsManager::instance();

  ASSERT_NO_THROW(
      usvfs->usvfsVirtualLinkFile((src / "b/b.txt").string(), mnt / "b.txt"));
  ASSERT_NO_THROW(usvfs->usvfsVirtualLinkDirectoryStatic((src / "a").string(), mnt, 0));
  ASSERT_TRUE(usvfs->mount());

  checkFileContent(mnt / "a.txt", "test a");
  checkFileContent(mnt / "a/a.txt", "test a/a");
  checkFileContent(mnt / "already_existed.txt", "test already_existed");
  checkFileContent(mnt / "already_existing_dir/already_existed0.txt",
                   "test already_existing_dir/already_existed0");
  checkFileContent(mnt / "b.txt", "test b");

  statDirectory(mnt / "empty_dir");

  EXPECT_TRUE(usvfs->unmount());

  EXPECT_TRUE(cleanup());
}
