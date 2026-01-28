#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <ranges>
#include <sys/statvfs.h>

#include "usvfs-fuse/usvfsmanager.h"

using namespace std;
namespace fs = std::filesystem;

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

const vector filesToCreate{
    pair{src / "a/a.txt", "test a"},
    pair{src / "a/a/a.txt", "test a/a"},
    pair{src / "b/b.txt", "test b"},
    pair{src / "c/c.txt", "test c"},
    pair{mnt / "already_existed.txt", "test already_existed"},
    pair{mnt / "already_existing_dir/already_existed0.txt",
         "test already_existing_dir/already_existed0"},
};

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
}

void dumpUsvfs()
{
  const auto usvfs = UsvfsManager::instance();
  const auto dump  = usvfs->usvfsCreateVFSDump();
  cout << "=============== usvfs dump ===============\n"
       << dump << "==========================================" << endl;
}

void openFile(const string& path)
{
  int fd;
  ASSERT_NE(fd = open(path.c_str(), O_RDONLY), -1)
      << "error opening file '" << path << "': " << strerror(errno);
  EXPECT_EQ(close(fd), 0) << "error closing file '" << path << "': " << strerror(errno);
}

void openFileWithFailure(const string& path, int error)
{
  int fd;
  EXPECT_EQ(fd = open(path.c_str(), O_RDONLY), -1)
      << "error opening file '" << path << "': " << strerror(errno);
  EXPECT_EQ(errno, error) << "expected " << strerrorname_np(error) << ", got "
                          << strerrorname_np(errno);
  if (fd != -1) {
    EXPECT_EQ(close(fd), 0) << "error closing file '" << path
                            << "': " << strerror(errno);
  }
}

void createDir(const string& path)
{
  EXPECT_EQ(mkdir(path.c_str(), mode), 0) << "error: " << strerror(errno);
}

void createDirWithFailure(const string& path, int error)
{
  EXPECT_EQ(mkdir(path.c_str(), mode), -1);
  EXPECT_EQ(errno, error) << "expected " << strerrorname_np(error) << ", got "
                          << strerrorname_np(errno);
}

void unlinkFile(const string& path)
{
  ASSERT_EQ(unlink(path.c_str()), 0)
      << "error unlinking file '" << path << "': " << strerror(errno);

  openFileWithFailure(path, ENOENT);
}

void unlinkFileWithFailure(const string& path, int error)
{
  EXPECT_EQ(unlink(path.c_str()), -1) << "error: " << strerror(errno);
  EXPECT_EQ(errno, error) << "expected " << strerrorname_np(error) << ", got "
                          << strerrorname_np(errno);
}

void unlinkDir(const string& path)
{
  ASSERT_EQ(rmdir(path.c_str()), 0) << "error: " << strerror(errno);

  openFileWithFailure(path, ENOENT);
}

void unlinkDirWithFailure(const string& path, int error)
{
  EXPECT_EQ(rmdir(path.c_str()), -1) << "error: " << strerror(errno);
  EXPECT_EQ(errno, error) << "expected " << strerrorname_np(error) << ", got "
                          << strerrorname_np(errno);
}

void statPath(const string& path)
{
  struct stat st{};
  EXPECT_EQ(stat(path.c_str(), &st), 0) << "error: " << strerror(errno);
}

void statPathWithFailure(const string& path, int error)
{
  struct stat st{};
  EXPECT_EQ(stat(path.c_str(), &st), -1);
  EXPECT_EQ(errno, error) << "expected " << strerrorname_np(error) << ", got "
                          << strerrorname_np(errno);
}

void readFile(const string& path, const string& expectedContent)
{
  int fd;
  ASSERT_NE(fd = open(path.c_str(), O_RDONLY), -1) << "error: " << strerror(errno);

  array<char, 4096> buf{};
  const ssize_t readBytes = read(fd, buf.data(), buf.size());
  int closeResult         = close(fd);
  EXPECT_NE(readBytes, -1) << "read error in file '" << path
                           << "': " << strerror(errno);

  EXPECT_EQ(closeResult, 0) << "error closing file '" << path
                            << "': " << strerror(errno);

  EXPECT_EQ(string(buf.data(), readBytes), expectedContent);
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

    usvfs->setUpperDir(upper);
    ASSERT_TRUE(usvfs->usvfsVirtualLinkDirectoryStatic(
        (src / "a").string(), mnt.string(), linkFlag::RECURSIVE));
    ASSERT_TRUE(usvfs->usvfsVirtualLinkDirectoryStatic(
        (src / "b").string(), mnt.string(), linkFlag::RECURSIVE));
    ASSERT_TRUE(usvfs->usvfsVirtualLinkFile("/tmp/usvfs/src/c/c.txt",
                                            "/tmp/usvfs/mnt2/c.txt", 0));

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
  ASSERT_FALSE(HasFailure());
}

TEST_F(UsvfsTest, getattr)
{
  const vector pathsToStat = {
      mnt / "a",
      mnt / "a.txt",
      mnt / "a/a.txt",
      mnt / "b.txt",
      mnt2 / "c.txt",
      mnt / "empty_dir",
      mnt / "already_existed.txt",
      mnt / "already_existing_dir",
      mnt / "already_existing_dir/already_existed0.txt",
  };

  for (const auto& filePath : pathsToStat) {
    statPath(filePath);
  }

  statPathWithFailure(mnt / "DOES_NOT_EXIST", ENOENT);
}

TEST_F(UsvfsTest, getattrCaseInsensitive)
{
  const vector pathsToStat = {
      mnt / "A",
      mnt / "A.txt",
      mnt / "A/A.txt",
      mnt / "B.txt",
      mnt2 / "C.txt",
      mnt / "EMPTY_DIR",
      mnt / "ALREADY_EXISTED.txt",
      mnt / "ALREADY_EXISTING_DIR",
      mnt / "ALREADY_EXISTING_DIR/ALREADY_EXISTED0.txt",
  };

  for (const auto& filePath : pathsToStat) {
    statPath(filePath);
  }

  statPathWithFailure(mnt / "DOES_NOT_EXIST", ENOENT);
}

TEST_F(UsvfsTest, open)
{
  for (const auto& file : filesToCheck | views::keys) {
    openFile(file.string());
  }

  openFileWithFailure(mnt / "DOES_NOT_EXIST", ENOENT);
}

TEST_F(UsvfsTest, openCaseInsensitive)
{
  for (const auto& file : filesToCheckCaseInsensitive | views::keys) {
    openFile(file.string());
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
    readFile(filePath, content);
  }
}

TEST_F(UsvfsTest, readCaseInsensitive)
{
  for (const auto& [filePath, content] : filesToCheckCaseInsensitive) {
    readFile(filePath, content);
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

  readFile(mnt / "asdf.txt", "test a");

  // opening the original file should fail with ENOENT
  openFileWithFailure(mnt / "a.txt", ENOENT);
}

TEST_F(UsvfsTest, renameCaseInsensitive)
{
  EXPECT_EQ(rename((mnt / "A.txt").c_str(), (mnt / "ASDF.txt").c_str()), 0)
      << "error: " << strerror(errno);

  readFile(mnt / "asdf.TXT", "test a");

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
  createFile(mnt / "a/new_file.txt", mnt / "A/NEW_FILE.TXT");

  ASSERT_EQ(mkdir((mnt / "NEW_DIR").c_str(), mode), 0) << "error: " << strerror(errno);
  int fd;
  ASSERT_GT(fd = open((mnt / "new_dir/testfile.txt").c_str(), oflags, mode), -1)
      << "error: " << strerror(errno);
  EXPECT_EQ(close(fd), 0);
}

TEST_F(UsvfsTest, statfs)
{
  struct statvfs buf;
  EXPECT_GT(statvfs(mnt.c_str(), &buf), -1) << "error: " << strerror(errno);
}

TEST(usvfs, CreateProcessHooked)
{
  initLogging();
  ASSERT_TRUE(createTmpDirs());

  auto usvfs = UsvfsManager::instance();
  usvfs->setProcessDelay(10ms);

  ASSERT_TRUE(usvfs->usvfsVirtualLinkDirectoryStatic((src / "a").string(), mnt.string(),
                                                     linkFlag::RECURSIVE));
  ASSERT_TRUE(usvfs->usvfsVirtualLinkDirectoryStatic((src / "b").string(), mnt.string(),
                                                     linkFlag::RECURSIVE));
  ASSERT_TRUE(
      usvfs->usvfsVirtualLinkFile("/tmp/usvfs/src/c/c.txt", "/tmp/usvfs/mnt/c.txt", 0));

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

TEST(usvfs, CreateProcessHooked_WithMountNamespace)
{
  initLogging();
  ASSERT_TRUE(createTmpDirs());

  auto usvfs = UsvfsManager::instance();
  usvfs->setProcessDelay(10ms);
  usvfs->setUseMountNamespace(true);

  ASSERT_TRUE(usvfs->usvfsVirtualLinkDirectoryStatic((src / "a").string(), mnt.string(),
                                                     linkFlag::RECURSIVE));
  ASSERT_TRUE(usvfs->usvfsVirtualLinkDirectoryStatic((src / "b").string(), mnt.string(),
                                                     linkFlag::RECURSIVE));
  ASSERT_TRUE(
      usvfs->usvfsVirtualLinkFile("/tmp/usvfs/src/c/c.txt", "/tmp/usvfs/mnt/c.txt", 0));

  pid_t pid = usvfs->usvfsCreateProcessHooked("tree", ".", mnt.string());
  ASSERT_GE(pid, 0);

  int status;
  EXPECT_GE(waitpid(pid, &status, 0), 0) << "error: " << strerror(errno);
  EXPECT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 0);

  EXPECT_TRUE(cleanup());
}
