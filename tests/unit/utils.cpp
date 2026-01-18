#include <gtest/gtest.h>

#include "../../src/utils.h"

using namespace std::string_literals;

TEST(utils, iequals)
{
  EXPECT_TRUE(iequals("tEsT", "test"));
  EXPECT_TRUE(iequals("TEST", "test"));
  EXPECT_TRUE(iequals("ÄÜöabC", "äüÖabc"));
  EXPECT_TRUE(iequals("TêśT", "tÊŚt"));
  EXPECT_TRUE(iequals("テストtest", "テストteSt"));
  EXPECT_TRUE(iequals("ЖЗИЙ", "жзий"));
  EXPECT_TRUE(iequals("ԱԲԳԴ", "աբգդ"));

  EXPECT_FALSE(iequals("TéśT", "tÊŚt"));
}

TEST(utils, istartsWith)
{
  EXPECT_TRUE(istartsWith("tEsT", "Te"));
  EXPECT_TRUE(istartsWith("TEST", "te"));
  EXPECT_TRUE(istartsWith("ÄÜötest", "äü"));
  EXPECT_TRUE(istartsWith("śTtest", "Śt"));
  EXPECT_TRUE(istartsWith("テストtest", "テス"));
}

TEST(utils, iendsWith)
{
  EXPECT_TRUE(iendsWith("tEsT", "St"));
  EXPECT_TRUE(iendsWith("TEST", "sT"));
  EXPECT_TRUE(iendsWith("testÄÜö", "üÖ"));
  EXPECT_TRUE(iendsWith("teśT", "Śt"));
  EXPECT_TRUE(iendsWith("テスト", "スト"));
}

TEST(utils, toLower)
{
  EXPECT_EQ(toLower("aBc"), "abc");
  EXPECT_EQ(toLower("ÄÜöabC"), "äüöabc");
  EXPECT_EQ(toLower("TÊŚT"), "têśt");
  EXPECT_EQ(toLower("テスト"), "テスト");
}

TEST(utils, toUpper)
{
  EXPECT_EQ(toUpper("aBc"), "ABC");
  EXPECT_EQ(toUpper("äüöabC"), "ÄÜÖABC");
  EXPECT_EQ(toUpper("têśt"), "TÊŚT");
  EXPECT_EQ(toUpper("テスト"), "テスト");
}

TEST(utils, getParentPath)
{
  EXPECT_EQ(getParentPath("/a"), "");
  EXPECT_EQ(getParentPath("/a"s), "");
  EXPECT_EQ(getParentPath("/a/b"), "/a");
  EXPECT_EQ(getParentPath("/a/b"s), "/a");
  EXPECT_EQ(getParentPath("/a/b/c"), "/a/b");
  EXPECT_EQ(getParentPath("/a/b/c"s), "/a/b");
}

TEST(utils, getFileNameFromPath)
{
  EXPECT_EQ(getFileNameFromPath("/a"), "a");
  EXPECT_EQ(getFileNameFromPath("/a"s), "a");
  EXPECT_EQ(getFileNameFromPath("/a/b"), "b");
  EXPECT_EQ(getFileNameFromPath("/a/b"s), "b");
  EXPECT_EQ(getFileNameFromPath("/a/b/c"), "c");
  EXPECT_EQ(getFileNameFromPath("/a/b/c"s), "c");
}
