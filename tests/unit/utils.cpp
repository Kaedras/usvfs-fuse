#include <gtest/gtest.h>

#include "../../src/utils.h"

#include <fcntl.h>

using namespace std;

TEST(utils, iequals)
{
  // ascii
  EXPECT_TRUE(iequals("tEsT", "test"));
  EXPECT_TRUE(iequals("TEST", "test"));
  // extended ascii
  EXPECT_TRUE(iequals("TêśT", "tÊŚt"));
  // umlaut
  EXPECT_TRUE(iequals("ÄÜö", "äüÖ"));
  // japanese
  EXPECT_TRUE(iequals("テスト", "テスト"));
  // cyrillic
  EXPECT_TRUE(iequals("ЖЗИЙ", "жзий"));
  // armenian
  EXPECT_TRUE(iequals("ԱԲԳԴ", "աբգդ"));
  // extended greek
  EXPECT_TRUE(iequals("Ἀ Ὥ Ἒ Ἧ", "ἀ ὥ ἒ ἧ"));

  EXPECT_FALSE(iequals("TéśT", "tÊŚt"));
}

TEST(utils, istartsWith)
{
  // ascii
  EXPECT_TRUE(istartsWith("tEsT", "Te"));
  EXPECT_TRUE(istartsWith("TEST", "te"));
  // extended ascii
  EXPECT_TRUE(istartsWith("tÊśt", "tê"));
  // umlaut
  EXPECT_TRUE(istartsWith("ÄÜö", "äü"));
  // japanese
  EXPECT_TRUE(istartsWith("テスト", "テス"));
  // cyrillic
  EXPECT_TRUE(istartsWith("ЖЗИЙ", "жз"));
  // armenian
  EXPECT_TRUE(istartsWith("ԱԲԳԴ", "աբ"));
  // extended greek
  EXPECT_TRUE(istartsWith("Ἀ Ὥ Ἒ Ἧ", "ἀ ὥ"));
}

TEST(utils, iendsWith)
{
  // ascii
  EXPECT_TRUE(iendsWith("tEsT", "St"));
  EXPECT_TRUE(iendsWith("TEST", "sT"));
  // extended ascii
  EXPECT_TRUE(iendsWith("tÊśt", "Śt"));
  // umlaut
  EXPECT_TRUE(iendsWith("ÄÜö", "üÖ"));
  // japanese
  EXPECT_TRUE(iendsWith("テスト", "スト"));
  // cyrillic
  EXPECT_TRUE(iendsWith("ЖЗИЙ", "ий"));
  // armenian
  EXPECT_TRUE(iendsWith("ԱԲԳԴ", "գդ"));
  // extended greek
  EXPECT_TRUE(iendsWith("Ἀ Ὥ Ἒ Ἧ", "ἒ ἧ"));
}

TEST(utils, toLower)
{
  // ascii
  EXPECT_EQ(toLower("tEsT"), "test");
  // extended ascii
  EXPECT_EQ(toLower("TÊŚT"), "têśt");
  // umlaut
  EXPECT_EQ(toLower("ÄÜö"), "äüö");
  // japanese
  EXPECT_EQ(toLower("テスト"), "テスト");
  // cyrillic
  EXPECT_EQ(toLower("ЖЗИЙ"), "жзий");
  // armenian
  EXPECT_EQ(toLower("ԱԲԳԴ"), "աբգդ");
  // extended greek
  EXPECT_EQ(toLower("Ἀ Ὥ Ἒ Ἧ"), "ἀ ὥ ἒ ἧ");
}

TEST(utils, toLowerInplace)
{
  auto test = [](const char* testString, const char* result) {
    string str = testString;
    toLowerInplace(str);
    EXPECT_EQ(str, result);
  };

  // ascii
  test("tEsT", "test");
  // extended ascii
  test("tÊśt", "têśt");
  // umlaut
  test("ÄÜö", "äüö");
  // japanese
  test("テスト", "テスト");
  // cyrillic
  test("ЖЗИЙ", "жзий");
  // armenian
  test("ԱԲԳԴ", "աբգդ");
  // extended greek
  test("Ἀ Ὥ Ἒ Ἧ", "ἀ ὥ ἒ ἧ");
}

TEST(utils, toUpper)
{
  // ascii
  EXPECT_EQ(toUpper("tEsT"), "TEST");
  // extended ascii
  EXPECT_EQ(toUpper("tÊśt"), "TÊŚT");
  // umlaut
  EXPECT_EQ(toUpper("äÜö"), "ÄÜÖ");
  // japanese
  EXPECT_EQ(toUpper("テスト"), "テスト");
  // cyrillic
  EXPECT_EQ(toUpper("жзий"), "ЖЗИЙ");
  // armenian
  EXPECT_EQ(toUpper("աբգդ"), "ԱԲԳԴ");
  // extended greek
  EXPECT_EQ(toUpper("ἀ ὥ ἒ ἧ"), "Ἀ Ὥ Ἒ Ἧ");
}

TEST(utils, toUpperInplace)
{
  auto test = [](const char* testString, const char* result) {
    string str = testString;
    toUpperInplace(str);
    EXPECT_EQ(str, result);
  };

  // ascii
  test("tEsT", "TEST");
  // extended ascii
  test("tÊśt", "TÊŚT");
  // umlaut
  test("äÜö", "ÄÜÖ");
  // japanese
  test("テスト", "テスト");
  // cyrillic
  test("жзий", "ЖЗИЙ");
  // armenian
  test("աբգդ", "ԱԲԳԴ");
  // extended greek
  test("ἀ ὥ ἒ ἧ", "Ἀ Ὥ Ἒ Ἧ");
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

TEST(utils, isParentPathOf)
{
  EXPECT_TRUE(isParentPathOf("/", "/a"));
  EXPECT_TRUE(isParentPathOf("/a", "/a"));
  EXPECT_TRUE(isParentPathOf("/a", "/a/"));
  EXPECT_TRUE(isParentPathOf("/a/", "/a"));
  EXPECT_TRUE(isParentPathOf("/a", "/a/b"));
  EXPECT_FALSE(isParentPathOf("/a", "/aa"));
  EXPECT_FALSE(isParentPathOf("/aa", "/a"));

  EXPECT_TRUE(isParentPathOf("/", "/A"));
  EXPECT_TRUE(isParentPathOf("/a", "/A"));
  EXPECT_TRUE(isParentPathOf("/a", "/A/"));
  EXPECT_TRUE(isParentPathOf("/a/", "/A"));
  EXPECT_TRUE(isParentPathOf("/a", "/A/B"));
  EXPECT_FALSE(isParentPathOf("/a", "/AA"));
  EXPECT_FALSE(isParentPathOf("/aa", "/A"));
}

TEST(utils, relativePath)
{
  EXPECT_EQ(relativePath("/a", "/a"), "");
  EXPECT_EQ(relativePath("/a/bc/", "/a"), "bc");
  EXPECT_EQ(relativePath("/a/bc", "/a/"), "bc");
  EXPECT_EQ(relativePath("/a/bc/", "/a/"), "bc");
}

TEST(utils, openFlagsToString)
{
  EXPECT_EQ(openFlagsToString(0), "");
  EXPECT_EQ(openFlagsToString(O_APPEND), "O_APPEND");
  EXPECT_EQ(openFlagsToString(O_APPEND | O_CREAT), "O_APPEND | O_CREAT");
  EXPECT_EQ(openFlagsToString(O_APPEND | O_CREAT | O_DIRECT | O_EXCL),
            "O_APPEND | O_CREAT | O_DIRECT | O_EXCL");
}
