#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sys/statvfs.h>

#include "usvfs-fuse/usvfsmanager.h"

using namespace std;
namespace fs = std::filesystem;

static constexpr auto delayAfterMount = 10ms;
static constexpr mode_t mode          = 0755;
static LogLevel logLevel              = LogLevel::Warning;
static constexpr bool enableDebugMode = false;

static const fs::path base  = fs::temp_directory_path() / "usvfs";
static const fs::path src   = base / "src";
static const fs::path mnt   = base / "mnt";
static const fs::path upper = base / "upper";

static const vector filesToCheck = {
    pair{mnt / "0.txt", "hello 0"},
    pair{mnt / "0/0.txt", "hello 0/0"},
    pair{mnt / "1.txt", "hello 1"},
    pair{mnt / "2.txt", "hello 2"},
    pair{mnt / "already_existed.txt", "hello from the other side"},
    pair{mnt / "already_existing_dir/already_existed0.txt",
         "hello 0 from the other side"},
};

static const vector filesToCreate{
    pair{src / "0/0.txt", "hello 0"},
    pair{src / "0/0/0.txt", "hello 0/0"},
    pair{src / "1/1.txt", "hello 1"},
    pair{src / "2/2.txt", "hello 2"},
    pair{mnt / "already_existed.txt", "hello from the other side"},
    pair{mnt / "already_existing_dir/already_existed0.txt",
         "hello 0 from the other side"},
};

static const vector srcDirsToCreate{src / "0",           src / "1",
                                    src / "2",           src / "0/0/",
                                    src / "0/empty_dir", mnt / "already_existing_dir"};

bool createTmpDirs()
{
  std::error_code ec;
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
  cout << "running " << cmd << endl;
  const int result = system(cmd.c_str());
  return WIFEXITED(result) && WEXITSTATUS(result) == 0;
}

void initLogging()
{

  auto usvfs = UsvfsManager::instance();
  usvfs->setLogLevel(logLevel);
}

string readFile(const fs::path& path)
{
  const auto usvfs = UsvfsManager::instance();

  const int fd = open(path.string().c_str(), O_RDONLY);
  if (fd == -1) {
    return "open failed: "s + strerror(errno);
  }

  array<char, 4096> buf{};
  const ssize_t readBytes = read(fd, buf.data(), buf.size());
  int closeResult         = close(fd);
  if (readBytes == -1) {
    return "read failed: "s + strerror(errno);
  }
  if (closeResult != 0) {
    return "close failed: "s + strerror(errno);
  }
  return string(buf.data(), readBytes);
}

void dumpUsvfs()
{
  const auto usvfs = UsvfsManager::instance();
  const auto dump  = usvfs->usvfsCreateVFSDump();
  cout << "=============== usvfs dump ===============\n"
       << dump << "==========================================" << endl;
}

class UsvfsTest : public testing::Test
{
protected:
  UsvfsTest() = default;
  void SetUp() override
  {
    initLogging();
    ASSERT_TRUE(createTmpDirs());
    // runCmd("tree "s + base.string());
    const auto usvfs = UsvfsManager::instance();
    if constexpr (enableDebugMode) {
      usvfs->enableDebugMode();
    }
    usvfs->setUpperDir(upper);
    ASSERT_TRUE(usvfs->usvfsVirtualLinkDirectoryStatic((src / "0").c_str(), mnt.c_str(),
                                                       linkFlag::RECURSIVE));
    ASSERT_TRUE(usvfs->usvfsVirtualLinkDirectoryStatic((src / "1").c_str(), mnt.c_str(),
                                                       linkFlag::RECURSIVE));
    ASSERT_TRUE(usvfs->usvfsVirtualLinkFile("/tmp/usvfs/src/2/2.txt",
                                            "/tmp/usvfs/mnt/2.txt", 0));

    ASSERT_NO_THROW(usvfs->mount());
    this_thread::sleep_for(delayAfterMount);
    // dumpUsvfs();
  }
  void TearDown() override
  {
    const auto usvfs = UsvfsManager::instance();
    ASSERT_TRUE(usvfs->unmount());
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
      mnt / "0",
      mnt / "0.txt",
      mnt / "0/0.txt",
      mnt / "1.txt",
      mnt / "2.txt",
      mnt / "empty_dir",
      mnt / "already_existed.txt",
      mnt / "already_existing_dir",
      mnt / "already_existing_dir/already_existed0.txt",
  };

  const auto usvfs = UsvfsManager::instance();

  struct stat st{};
  for (const auto& filePath : pathsToStat) {
    EXPECT_EQ(stat(filePath.c_str(), &st), 0) << "error: " << strerror(errno);
  }

  EXPECT_EQ(stat((mnt / "DOES_NOT_EXIST").c_str(), &st), -1);
  EXPECT_EQ(errno, ENOENT) << "expected ENOENT, got " << strerrorname_np(errno);
}

TEST_F(UsvfsTest, open)
{
  const auto usvfs = UsvfsManager::instance();

  int fd = open((mnt / "0.txt").c_str(), O_RDONLY);
  EXPECT_NE(fd, -1) << "error: " << strerror(errno);
  if (fd != -1) {
    EXPECT_EQ(close(fd), 0) << "error: " << strerror(errno);
  }

  fd = open((mnt / "already_existed.txt").c_str(), O_RDONLY);
  EXPECT_NE(fd, -1) << "error: " << strerror(errno);
  if (fd != -1) {
    EXPECT_EQ(close(fd), 0) << "error: " << strerror(errno);
  }

  fd          = open((mnt / "DOES_NOT_EXIST").c_str(), O_RDONLY);
  const int e = errno;
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(e, ENOENT) << "expected ENOENT, got " << strerrorname_np(e);
}

TEST_F(UsvfsTest, readdir)
{
  EXPECT_TRUE(runCmd("tree "s + mnt.c_str())) << "error: " << strerror(errno);
}

TEST_F(UsvfsTest, mkdir)
{
  const auto usvfs = UsvfsManager::instance();

  EXPECT_EQ(mkdir((mnt / "A").c_str(), mode), 0) << "error: " << strerror(errno);
  EXPECT_EQ(mkdir((mnt / "A/b").c_str(), mode), 0) << "error: " << strerror(errno);
  EXPECT_EQ(mkdir((mnt / "a/c").c_str(), mode), 0) << "error: " << strerror(errno);

  EXPECT_EQ(mkdir((mnt / "a").c_str(), mode), -1);
  EXPECT_EQ(errno, EEXIST) << "expected EEXIST, got " << strerrorname_np(errno);

  EXPECT_EQ(mkdir((mnt / "b/c/d/e").c_str(), mode), -1);
  EXPECT_EQ(errno, ENOENT) << "expected ENOENT, got " << strerrorname_np(errno);
}

TEST_F(UsvfsTest, read)
{
  for (const auto& [filePath, content] : filesToCheck) {
    EXPECT_EQ(readFile(filePath), content);
  }
}

TEST_F(UsvfsTest, unlink)
{
  const auto usvfs = UsvfsManager::instance();

  EXPECT_EQ(unlink((mnt / "0.txt").c_str()), 0) << "error: " << strerror(errno);
  EXPECT_EQ(unlink((mnt / "already_existed.txt").c_str()), 0)
      << "error: " << strerror(errno);
  // check if the files have been removed
  EXPECT_EQ(open((mnt / "0.txt").c_str(), O_RDONLY), -1);
  EXPECT_EQ(errno, ENOENT) << "expected ENOENT, got " << strerrorname_np(errno);
  EXPECT_EQ(open((mnt / "already_existed.txt").c_str(), O_RDONLY), -1);
  EXPECT_EQ(errno, ENOENT) << "expected ENOENT, got " << strerrorname_np(errno);

  EXPECT_EQ(rmdir((mnt / "empty_dir").c_str()), 0) << "error: " << strerror(errno);
  // check if the directory has been removed
  EXPECT_EQ(open((mnt / "empty_dir").c_str(), O_RDONLY), -1);
  EXPECT_EQ(errno, ENOENT) << "expected ENOENT, got " << strerrorname_np(errno);

  EXPECT_EQ(unlink((mnt / "0").c_str()), -1);
  EXPECT_EQ(errno, EISDIR) << "expected EISDIR, got " << strerrorname_np(errno);

  EXPECT_EQ(rmdir((mnt / "0").c_str()), -1);
  EXPECT_EQ(errno, ENOTEMPTY) << "expected ENOTEMPTY, got " << strerrorname_np(errno);

  EXPECT_TRUE(runCmd("rm -rf "s + mnt.c_str() + "/0"));
}

TEST_F(UsvfsTest, rename)
{
  EXPECT_EQ(rename((mnt / "0.txt").c_str(), (mnt / "asdf.txt").c_str()), 0)
      << "error: " << strerror(errno);

  EXPECT_EQ(readFile(mnt / "asdf.txt"), "hello 0");
  EXPECT_EQ(open((mnt / "0.txt").c_str(), O_RDONLY), -1);
}

TEST_F(UsvfsTest, chmod)
{
  fs::path file = mnt / "0.txt";
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
  auto path = mnt / "testfile.txt";
  int fd    = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, mode);
  EXPECT_GT(fd, -1) << "error: " << strerror(errno);
  if (fd >= 0) {
    EXPECT_EQ(close(fd), 0);
  }

  EXPECT_EQ(open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, mode), -1);
  EXPECT_EQ(errno, EEXIST) << "expected EEXIST, got " << strerrorname_np(errno);

  EXPECT_EQ(mkdir((mnt / "A").c_str(), mode), 0) << "error: " << strerror(errno);
  fd = open((mnt / "a/testfile.txt").c_str(), O_WRONLY | O_CREAT | O_EXCL, mode);
  EXPECT_GT(fd, -1) << "error: " << strerror(errno);
  if (fd >= 0) {
    EXPECT_EQ(close(fd), 0);
  }
}

TEST_F(UsvfsTest, statfs)
{
  struct statvfs buf;
  EXPECT_GT(statvfs(mnt.c_str(), &buf), -1) << "error: " << strerror(errno);
}
