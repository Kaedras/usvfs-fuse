#include <filesystem>
#include <gtest/gtest.h>
#include <ostream>

#include "../../src/virtualfiletreeitem.h"
#include "usvfs-fuse/logging.h"
#include "usvfs-fuse/usvfsmanager.h"

using namespace std;
using filesystem::file_type;

static LogLevel logLevel = LogLevel::Warning;

class FileTreeTest : public testing::Test
{
protected:
  FileTreeTest() = default;
  void SetUp() override
  {
    initLogging();
    fileTree = VirtualFileTreeItem::create("/", "/tmp", dir);
  }
  void TearDown() override {}

  static void initLogging()
  {
    auto usvfs = UsvfsManager::instance();
    usvfs->setLogLevel(logLevel);
  }
  static void addItems(shared_ptr<VirtualFileTreeItem>& tree)
  {
    ASSERT_TRUE(tree->add("/1", "/tmp/a", dir));
    ASSERT_TRUE(tree->add("/1/1", "/tmp/a/a", dir));
    ASSERT_TRUE(tree->add("/2", "/tmp/b", dir));
    ASSERT_TRUE(tree->add("/2/1", "/tmp/b/a", dir));
    ASSERT_TRUE(tree->add("/2/2", "/tmp/b/b", dir));
    ASSERT_TRUE(tree->add("/2/2/1", "/tmp/b/b/a", dir));
    ASSERT_TRUE(tree->add("/2/3", "/tmp/b/c", dir));
    ASSERT_TRUE(tree->add("/3", "/tmp/c", dir));
    ASSERT_TRUE(tree->add("/3/1", "/tmp/c/a", dir));
    ASSERT_TRUE(tree->add("/3/2", "/tmp/c/b", dir));
    ASSERT_TRUE(tree->add("/3/2/1", "/tmp/c/b/a", dir));
  }
  static void addItemsNonASCII(shared_ptr<VirtualFileTreeItem>& tree)
  {
    ASSERT_TRUE(tree->add("Ä", "/tmp/Ö", dir));
    ASSERT_TRUE(tree->add("こんいちわ", "/tmp/テスト", dir));
  }
  static string find(shared_ptr<VirtualFileTreeItem>& tree, string_view value)
  {
    if (const auto result = tree->find(value)) {
      return result->realPath();
    }
    return "";
  }

  void addItems() { addItems(fileTree); }
  void addItemsNonASCII() { addItemsNonASCII(fileTree); }
  string find(string_view value) { return find(fileTree, value); }

  shared_ptr<VirtualFileTreeItem> fileTree;
};

TEST_F(FileTreeTest, Add)
{
  addItems();

  // adding an existing path should fail with EEXIST
  ASSERT_FALSE(fileTree->add("/3", "/tmp/c", file));
  EXPECT_EQ(errno, EEXIST);
}

TEST_F(FileTreeTest, AddNonASCII)
{
  addItemsNonASCII();

  // adding an existing path should fail with EEXIST
  ASSERT_FALSE(fileTree->add("/こんいちわ", "/tmp/テスト", file));
  EXPECT_EQ(errno, EEXIST);
}

// this not only tests printing the tree, but also whether the elements were added
// correctly
TEST_F(FileTreeTest, PrintTree)
{
  addItems();

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
  s << fileTree;
  EXPECT_EQ(s.str(), expectedResult);
}

TEST_F(FileTreeTest, PrintTreeNonASCII)
{
  addItemsNonASCII();

  static const string expectedResult =
      "file path: \"\", real path: \"/tmp\"\n"
      "file path: \"/Ä\", real path: \"/tmp/Ö\"\n"
      "file path: \"/こんいちわ\", real path: \"/tmp/テスト\"\n";

  stringstream s;
  s << fileTree;
  EXPECT_EQ(s.str(), expectedResult);
}

TEST_F(FileTreeTest, DumpTree)
{
  addItems();

  static const string expectedResult = "/ -> /tmp\n"
                                       " 1/ -> /tmp/a\n"
                                       "  1/ -> /tmp/a/a\n"
                                       " 2/ -> /tmp/b\n"
                                       "  1/ -> /tmp/b/a\n"
                                       "  2/ -> /tmp/b/b\n"
                                       "   1/ -> /tmp/b/b/a\n"
                                       "  3/ -> /tmp/b/c\n"
                                       " 3/ -> /tmp/c\n"
                                       "  1/ -> /tmp/c/a\n"
                                       "  2/ -> /tmp/c/b\n"
                                       "   1/ -> /tmp/c/b/a\n";

  stringstream s;
  fileTree->dumpTree(s);
  EXPECT_EQ(s.str(), expectedResult);
}

TEST_F(FileTreeTest, DumpTreeNonASCII)
{
  addItemsNonASCII();

  static const string expectedResult = "/ -> /tmp\n"
                                       " Ä/ -> /tmp/Ö\n"
                                       " こんいちわ/ -> /tmp/テスト\n";

  stringstream s;
  fileTree->dumpTree(s);
  EXPECT_EQ(s.str(), expectedResult);
}

TEST_F(FileTreeTest, Find)
{
  addItems();

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

TEST_F(FileTreeTest, FindNonASCII)
{
  addItemsNonASCII();

  EXPECT_EQ(find("/Ä"), "/tmp/Ö");
  EXPECT_EQ(find("こんいちわ"), "/tmp/テスト");
}

TEST_F(FileTreeTest, OverwriteEntries)
{
  addItems();

  // overwrite items
  EXPECT_TRUE(fileTree->add("/1", "/tmp/A", dir, true));
  EXPECT_TRUE(fileTree->add("/1/1", "/tmp/A/A", dir, true));
  EXPECT_TRUE(fileTree->add("/2", "/tmp/B", dir, true));
  EXPECT_TRUE(fileTree->add("/2/1", "/tmp/B/A", file, true));
  EXPECT_TRUE(fileTree->add("/2/2", "/tmp/B/B", dir, true));
  EXPECT_TRUE(fileTree->add("/2/2/1", "/tmp/B/B/A", file, true));
  EXPECT_TRUE(fileTree->add("/2/2/1", "/tmp/b/b/abc", file, true));
  EXPECT_TRUE(fileTree->add("/2/3", "/tmp/B/C", file, true));
  EXPECT_TRUE(fileTree->add("/3", "/tmp/C", dir, true));

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
  fileTree->add("/1", "/tmp/1", file);
  fileTree->add("/2", "/tmp/2", file);
  fileTree->add("/3", "/tmp/3", dir);
  fileTree->add("/3/1", "/tmp/3/1", dir);
  fileTree->add("/3/1/1", "/tmp/3/1/1", dir);

  {
    auto newFileTree = VirtualFileTreeItem::create("/", "/tmp", dir);
    newFileTree->add("/1", "/tmp/A", dir);
    newFileTree->add("/3", "/tmp/3", dir);
    newFileTree->add("/3/1", "/tmp/3/1", dir);
    newFileTree->add("/3/1/1", "/tmp/3/1/1", dir);
    newFileTree->add("/3/1/1/1", "/tmp/3/1/1/1", dir);
    newFileTree->add("/3/2", "/tmp/3/2", dir);
    newFileTree->add("/4", "/tmp/4", dir);
    newFileTree->add("/4/4", "/tmp/4/4", dir);
    newFileTree->add("/4/4/4", "/tmp/4/4/4", dir);

    *fileTree += *newFileTree;
  }

  static const string expectedResult =
      "file path: \"\", real path: \"/tmp\"\n"
      "file path: \"/1\", real path: \"/tmp/A\"\n"
      "file path: \"/2\", real path: \"/tmp/2\"\n"
      "file path: \"/3\", real path: \"/tmp/3\"\n"
      "file path: \"/3/1\", real path: \"/tmp/3/1\"\n"
      "file path: \"/3/1/1\", real path: \"/tmp/3/1/1\"\n"
      "file path: \"/3/1/1/1\", real path: \"/tmp/3/1/1/1\"\n"
      "file path: \"/3/2\", real path: \"/tmp/3/2\"\n"
      "file path: \"/4\", real path: \"/tmp/4\"\n"
      "file path: \"/4/4\", real path: \"/tmp/4/4\"\n"
      "file path: \"/4/4/4\", real path: \"/tmp/4/4/4\"\n";

  stringstream ss;
  ss << fileTree;
  EXPECT_EQ(ss.str(), expectedResult);
}

TEST_F(FileTreeTest, CopyTree)
{
  addItems();
  shared_ptr<VirtualFileTreeItem> copy = fileTree->clone();
  fileTree.reset();

  EXPECT_EQ(find(copy, "/1"), "/tmp/a");
  EXPECT_EQ(find(copy, "/1/1"), "/tmp/a/a");
  EXPECT_EQ(find(copy, "/2"), "/tmp/b");
  EXPECT_EQ(find(copy, "/2/1"), "/tmp/b/a");
  EXPECT_EQ(find(copy, "/2/2"), "/tmp/b/b");
  EXPECT_EQ(find(copy, "/2/2/1"), "/tmp/b/b/a");
  EXPECT_EQ(find(copy, "/2/3"), "/tmp/b/c");
  EXPECT_EQ(find(copy, "/3"), "/tmp/c");
  EXPECT_EQ(find(copy, "/3/1"), "/tmp/c/a");
  EXPECT_EQ(find(copy, "/3/2"), "/tmp/c/b");
  EXPECT_EQ(find(copy, "/3/2/1"), "/tmp/c/b/a");

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

  stringstream ss;
  ss << copy;
  EXPECT_EQ(ss.str(), expectedResult);
}

TEST_F(FileTreeTest, Erase)
{
  addItems();

  ASSERT_TRUE(fileTree->erase("/1/1", false));
  ASSERT_EQ(fileTree->find("/1/1"), nullptr);
  ASSERT_NE(fileTree->find("/1/1", true), nullptr);

  // mark "/2" as deleted
  ASSERT_TRUE(fileTree->erase("/2", false));

  // check if "/2" is marked as deleted
  ASSERT_EQ(fileTree->find("/2"), nullptr);
  ASSERT_NE(fileTree->find("/2", true), nullptr);

  // check if children are also marked as deleted
  ASSERT_EQ(fileTree->find("/2/1"), nullptr);
  ASSERT_NE(fileTree->find("/2/1", true), nullptr);

  // delete "/2"
  ASSERT_TRUE(fileTree->erase("/2", true));
  // check if children have been deleted
  ASSERT_EQ(fileTree->find("/2/3", true), nullptr);
}

TEST_F(FileTreeTest, InsertAfterErase)
{
  addItems();

  ASSERT_TRUE(fileTree->erase("/1/1", true));
  ASSERT_NE(fileTree->add("/1/1", "/tmp/1/1"), nullptr);
  ASSERT_EQ(find("/1/1"), "/tmp/1/1");

  ASSERT_TRUE(fileTree->erase("/1/1", false));
  ASSERT_NE(fileTree->add("/1/1", "/tmp/A/A"), nullptr);
  ASSERT_EQ(find("/1/1"), "/tmp/A/A");
}
