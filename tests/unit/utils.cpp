#include <gtest/gtest.h>

#include "../../src/utils.h"

using namespace std;

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

TEST(utils, toLowerInplace)
{
  auto test = [](const char* testString, const char* result) {
    string str = testString;
    toLowerInplace(str);
    EXPECT_EQ(str, result);
  };

  test("aBc", "abc");
  test("ÄÜöabC", "äüöabc");
  test("TÊŚT", "têśt");
  test("テスト", "テスト");
}

TEST(utils, toUpper)
{
  EXPECT_EQ(toUpper("aBc"), "ABC");
  EXPECT_EQ(toUpper("äüöabC"), "ÄÜÖABC");
  EXPECT_EQ(toUpper("têśt"), "TÊŚT");
  EXPECT_EQ(toUpper("テスト"), "テスト");
}

TEST(utils, toUpperInplace)
{
  auto test = [](const char* testString, const char* result) {
    string str = testString;
    toUpperInplace(str);
    EXPECT_EQ(str, result);
  };

  test("aBc", "ABC");
  test("äüöabC", "ÄÜÖABC");
  test("têśt", "TÊŚT");
  test("テスト", "テスト");
}

TEST(utils, getParentPath)
{
  EXPECT_EQ(getParentPath("/a"), "");
  EXPECT_EQ(getParentPath("/a/b"), "/a");
  EXPECT_EQ(getParentPath("/a/b/c"), "/a/b");
}

TEST(utils, getFileNameFromPath)
{
  EXPECT_EQ(getFileNameFromPath("/a"), "a");
  EXPECT_EQ(getFileNameFromPath("/a/b"), "b");
  EXPECT_EQ(getFileNameFromPath("/a/b/c"), "c");
}
