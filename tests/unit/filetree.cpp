#include <filesystem>
#include <gtest/gtest.h>
#include <ostream>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>

#include "usvfs_fuse/virtualfiletreeitem.h"

using namespace std;
using filesystem::file_type;

static spdlog::level::level_enum logLevel = spdlog::level::warn;

class FileTreeTest : public testing::Test
{
protected:
  FileTreeTest() = default;
  void SetUp() override { initLogging(); }
  void TearDown() override {}

  void initLogging()
  {
    static constexpr auto loggerName = "usvfs";
    logger = spdlog::create<spdlog::sinks::stdout_sink_mt>(loggerName);
    logger->set_level(logLevel);
  }
  static void addItems(VirtualFileTreeItem& root)
  {
    root.add("/1", "/tmp/a", dir);
    root.add("/1/1", "/tmp/a/a", dir);
    root.add("/2", "/tmp/b", dir);
    root.add("/2/1", "/tmp/b/a", dir);
    root.add("/2/2", "/tmp/b/b", dir);
    root.add("/2/2/1", "/tmp/b/b/a", dir);
    root.add("/2/3", "/tmp/b/c", dir);
    root.add("/3", "/tmp/c", dir);
    root.add("/3/1", "/tmp/c/a", dir);
    root.add("/3/2", "/tmp/c/b", dir);
    root.add("/3/2/1", "/tmp/c/b/a", dir);
  }
  static void addItemsNonASCII(VirtualFileTreeItem& root)
  {
    root.add("Ä", "/tmp/Ö", dir);
    root.add("こんいちわ", "/tmp/テスト", dir);
  }

  std::shared_ptr<spdlog::logger> logger;
};

TEST_F(FileTreeTest, CanInsert)
{
  VirtualFileTreeItem root("/", "/tmp", dir);
  EXPECT_TRUE(root.add("/1", "/tmp/a", dir));
  EXPECT_TRUE(root.add("/1/1", "/tmp/a/a", file));
  EXPECT_TRUE(root.add("/2", "/tmp/b", dir));
  EXPECT_TRUE(root.add("/2/1", "/tmp/b/a", file));
  EXPECT_TRUE(root.add("/2/2", "/tmp/b/b", dir));
  EXPECT_TRUE(root.add("/2/2/1", "/tmp/b/b/a", file));
  EXPECT_TRUE(root.add("/2/3", "/tmp/b/c", file));
}

TEST_F(FileTreeTest, CanInsertNonASCII)
{
  VirtualFileTreeItem root("/", "/tmp", dir);

  EXPECT_TRUE(root.add("Ä", "/tmp/Ö", file));
  EXPECT_TRUE(root.add("こんいちわ", "/tmp/テスト", file));
}

// this not only tests printing the tree, but also whether the elements were added
// correctly
TEST_F(FileTreeTest, PrintTree)
{
  VirtualFileTreeItem root("/", "/tmp", dir);
  addItems(root);

  static const string expectedResult =
      "file path: \"\", real path: \"/tmp\"\n"
      "file path: \"/1\", real path: \"/tmp/a\"\n"
      "file path: \"/1/1\", real path: \"/tmp/a/a\"\n"
      "file path: \"/2\", real path: \"/tmp/b\"\n"
      "file path: \"/2/1\", real path: \"/tmp/b/a\"\n"
      "file path: \"/2/2\", real path: \"/tmp/b/b\"\n"
      "file path: \"/2/2/1\", real path: \"/tmp/b/b/a\"\n"
      "file path: \"/2/3\", real path: \"/tmp/b/c\"\n"
      "file path: \"/3\", real path: \"/tmp/c\"\n"
      "file path: \"/3/1\", real path: \"/tmp/c/a\"\n"
      "file path: \"/3/2\", real path: \"/tmp/c/b\"\n"
      "file path: \"/3/2/1\", real path: \"/tmp/c/b/a\"\n";

  stringstream s;
  s << root;
  EXPECT_EQ(s.str(), expectedResult);
}

TEST_F(FileTreeTest, PrintTreeNonASCII)
{
  VirtualFileTreeItem root("/", "/tmp", dir);
  addItemsNonASCII(root);

  static const string expectedResult =
      "file path: \"\", real path: \"/tmp\"\n"
      "file path: \"/Ä\", real path: \"/tmp/Ö\"\n"
      "file path: \"/こんいちわ\", real path: \"/tmp/テスト\"\n";

  stringstream s;
  s << root;
  EXPECT_EQ(s.str(), expectedResult);
}

TEST_F(FileTreeTest, CanFindInsertedItems)
{
  VirtualFileTreeItem root("/", "/tmp", dir);
  addItems(root);

  // helper function, required because find returns nullptr if the value has not been
  // found
  auto find = [&](const char* value) -> string {
    const VirtualFileTreeItem* result = root.find(value);
    if (result == nullptr) {
      return "";
    }
    return result->realPath();
  };

  EXPECT_EQ(find("/1"), "/tmp/a");
  EXPECT_EQ(find("/1/1"), "/tmp/a/a");
  EXPECT_EQ(find("/2"), "/tmp/b");
  EXPECT_EQ(find("/2/1"), "/tmp/b/a");
  EXPECT_EQ(find("/2/2"), "/tmp/b/b");
  EXPECT_EQ(find("/2/2/1"), "/tmp/b/b/a");
  EXPECT_EQ(find("/2/3"), "/tmp/b/c");
  EXPECT_EQ(find("/3"), "/tmp/c");
  EXPECT_EQ(find("/3/1"), "/tmp/c/a");
  EXPECT_EQ(find("/3/2"), "/tmp/c/b");
  EXPECT_EQ(find("/3/2/1"), "/tmp/c/b/a");
}

TEST_F(FileTreeTest, CanFindInsertedItemsNonASCII)
{
  VirtualFileTreeItem root("/", "/tmp", dir);
  addItemsNonASCII(root);

  // helper function, required because find returns nullptr if the value has not been
  // found
  auto find = [&root](const char* value) -> string {
    const VirtualFileTreeItem* result = root.find(value);
    if (result == nullptr) {
      return "";
    }
    return result->realPath();
  };

  EXPECT_EQ(find("/Ä"), "/tmp/Ö");
  EXPECT_EQ(find("こんいちわ"), "/tmp/テスト");
}

TEST_F(FileTreeTest, CanOverwriteEntries)
{
  VirtualFileTreeItem root("/", "/tmp", dir);
  addItems(root);

  // overwrite items
  EXPECT_TRUE(root.add("/1", "/tmp/A", dir, true));
  EXPECT_TRUE(root.add("/1/1", "/tmp/A/A", dir, true));
  EXPECT_TRUE(root.add("/2", "/tmp/B", dir, true));
  EXPECT_TRUE(root.add("/2/1", "/tmp/B/A", file, true));
  EXPECT_TRUE(root.add("/2/2", "/tmp/B/B", dir, true));
  EXPECT_TRUE(root.add("/2/2/1", "/tmp/B/B/A", file, true));
  EXPECT_TRUE(root.add("/2/2/1", "/tmp/b/b/abc", file, true));
  EXPECT_TRUE(root.add("/2/3", "/tmp/B/C", file, true));
  EXPECT_TRUE(root.add("/3", "/tmp/C", dir, true));

  // helper function, required because find returns nullptr if the value has not been
  // found
  auto find = [&](const char* value) -> string {
    const VirtualFileTreeItem* result = root.find(value);
    if (result == nullptr) {
      return "";
    }
    return result->realPath();
  };

  EXPECT_EQ(find("/1"), "/tmp/A");
  EXPECT_EQ(find("/1/1"), "/tmp/A/A");
  EXPECT_EQ(find("/2"), "/tmp/B");
  EXPECT_EQ(find("/2/1"), "/tmp/B/A");
  EXPECT_EQ(find("/2/2"), "/tmp/B/B");
  EXPECT_EQ(find("/2/2/1"), "/tmp/b/b/abc");
  EXPECT_EQ(find("/2/3"), "/tmp/B/C");
  EXPECT_EQ(find("/3"), "/tmp/C");
  EXPECT_EQ(find("/3/1"), "/tmp/c/a");
  EXPECT_EQ(find("/3/2"), "/tmp/c/b");
}

TEST_F(FileTreeTest, MergeTrees)
{
  VirtualFileTreeItem root("/", "/tmp", dir);
  root.add("/1", "/tmp/1", file);
  root.add("/2", "/tmp/2", file);
  root.add("/3", "/tmp/3", dir);
  root.add("/3/1", "/tmp/3/1", file);

  VirtualFileTreeItem root2("/", "/tmp", dir);
  root2.add("/1", "/tmp/A", dir);
  root2.add("/3", "/tmp/3", dir);
  root2.add("/3/2", "/tmp/3/2", dir);
  root2.add("/4", "/tmp/4", dir);
  root2.add("/4/4", "/tmp/4/4", dir);
  root2.add("/4/4/4", "/tmp/4/4/4", dir);

  root += root2;

  static const string expectedResult =
      "file path: \"\", real path: \"/tmp\"\n"
      "file path: \"/1\", real path: \"/tmp/A\"\n"
      "file path: \"/2\", real path: \"/tmp/2\"\n"
      "file path: \"/3\", real path: \"/tmp/3\"\n"
      "file path: \"/3/1\", real path: \"/tmp/3/1\"\n"
      "file path: \"/3/2\", real path: \"/tmp/3/2\"\n"
      "file path: \"/4\", real path: \"/tmp/4\"\n"
      "file path: \"/4/4\", real path: \"/tmp/4/4\"\n"
      "file path: \"/4/4/4\", real path: \"/tmp/4/4/4\"\n";

  stringstream ss;
  ss << root;
  EXPECT_EQ(ss.str(), expectedResult);
}
