#include <gtest/gtest.h>

#include "../../src/utils.h"

#include <fcntl.h>

using namespace std;

TEST(utils, iequals)
{
  // ascii
  EXPECT_TRUE(iequals("tEsT", "test"));
  EXPECT_TRUE(iequals("TEST", "test"));
  EXPECT_TRUE(iequals("TEST123!@#", "test123!@#"));
  EXPECT_TRUE(iequals("   \t\n  ", "   \t\n  "));
  EXPECT_TRUE(iequals("", ""));
  EXPECT_FALSE(iequals("", "a"));
  EXPECT_FALSE(iequals("a", ""));
  // different lengths
  EXPECT_FALSE(iequals("test", "testing"));
  EXPECT_FALSE(iequals("testing", "test"));

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
  EXPECT_TRUE(istartsWith("a", ""));
  EXPECT_TRUE(istartsWith("", ""));
  EXPECT_FALSE(istartsWith("", "a"));
  // longer prefix than string
  EXPECT_FALSE(istartsWith("test", "testing"));
  // empty prefix
  EXPECT_TRUE(istartsWith("test", ""));

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
  EXPECT_TRUE(iendsWith("a", ""));
  EXPECT_TRUE(iendsWith("", ""));
  EXPECT_FALSE(iendsWith("", "a"));
  // longer suffix than string
  EXPECT_FALSE(iendsWith("test", "testing"));
  // empty suffix
  EXPECT_TRUE(iendsWith("test", ""));

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
  EXPECT_EQ(toLower("TEST123!@#"), "test123!@#");
  EXPECT_EQ(toLower("   \t\n  "), "   \t\n  ");
  EXPECT_EQ(toLower(""), "");
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
#define RUN(testString, result)                                                        \
  {                                                                                    \
    string str = testString;                                                           \
    toLowerInplace(str);                                                               \
    EXPECT_EQ(str, result);                                                            \
  }
  // ascii
  RUN("tEsT", "test");
  RUN("TEST123!@#", "test123!@#");
  RUN("   \t\n  ", "   \t\n  ");
  RUN("", "");
  // extended ascii
  RUN("tÊśt", "têśt");
  // umlaut
  RUN("ÄÜö", "äüö");
  // japanese
  RUN("テスト", "テスト");
  // cyrillic
  RUN("ЖЗИЙ", "жзий");
  // armenian
  RUN("ԱԲԳԴ", "աբգդ");
  // extended greek
  RUN("Ἀ Ὥ Ἒ Ἧ", "ἀ ὥ ἒ ἧ");
#undef RUN
}

TEST(utils, toUpper)
{
  // ascii
  EXPECT_EQ(toUpper("tEsT"), "TEST");
  EXPECT_EQ(toUpper("test123!@#"), "TEST123!@#");
  EXPECT_EQ(toUpper("   \t\n  "), "   \t\n  ");
  EXPECT_EQ(toUpper(""), "");
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
#define RUN(testString, result)                                                        \
  {                                                                                    \
    string str = testString;                                                           \
    toUpperInplace(str);                                                               \
    EXPECT_EQ(str, result);                                                            \
  }
  // ascii
  RUN("tEsT", "TEST");
  RUN("test123!@#", "TEST123!@#");
  RUN("   \t\n  ", "   \t\n  ");
  RUN("", "");
  // extended ascii
  RUN("tÊśt", "TÊŚT");
  // umlaut
  RUN("äÜö", "ÄÜÖ");
  // japanese
  RUN("テスト", "テスト");
  // cyrillic
  RUN("жзий", "ЖЗИЙ");
  // armenian
  RUN("աբգդ", "ԱԲԳԴ");
  // extended greek
  RUN("ἀ ὥ ἒ ἧ", "Ἀ Ὥ Ἒ Ἧ");
#undef RUN
}

TEST(utils, getParentPath)
{
  EXPECT_EQ(getParentPath("/"), "");
  EXPECT_EQ(getParentPath("/a"), "");
  EXPECT_EQ(getParentPath("/a/b"), "/a");
  EXPECT_EQ(getParentPath("/a/b/c"), "/a/b");
  EXPECT_EQ(getParentPath("a/b/c"), "a/b");
  EXPECT_EQ(getParentPath("/a/b/c/d"), "/a/b/c");
  EXPECT_EQ(getParentPath("/a/b/c/d/"), "/a/b/c/d");
}

TEST(utils, getFileNameFromPath)
{
  EXPECT_EQ(getFileNameFromPath("/"), "");
  EXPECT_EQ(getFileNameFromPath(""), "");
  EXPECT_EQ(getFileNameFromPath("/a"), "a");
  EXPECT_EQ(getFileNameFromPath("/a/b"), "b");
  EXPECT_EQ(getFileNameFromPath("/a/b/c"), "c");
  EXPECT_EQ(getFileNameFromPath("a/b/c"), "c");
}

TEST(utils, isParentPathOf)
{
  EXPECT_TRUE(isParentPathOf("", ""));
  EXPECT_TRUE(isParentPathOf("", "/"));
  EXPECT_TRUE(isParentPathOf("/", ""));
  EXPECT_TRUE(isParentPathOf("/", "/"));
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
  EXPECT_EQ(relativePath("", ""), "");
  EXPECT_EQ(relativePath("/", "/"), "");
  EXPECT_EQ(relativePath("/a", "/a"), "");
  EXPECT_EQ(relativePath("/a/b", "/a"), "b");
  EXPECT_EQ(relativePath("/a/b/", "/a"), "b");
  EXPECT_EQ(relativePath("/a/b/c", "/a/b"), "c");
  EXPECT_EQ(relativePath("/a/b/c/", "/a/b"), "c");
  EXPECT_EQ(relativePath("/a/b/c", "/a"), "b/c");
}

TEST(utils, openFlagsToString)
{
  EXPECT_EQ(openFlagsToString(0), "");
  EXPECT_EQ(openFlagsToString(O_APPEND), "O_APPEND");
  EXPECT_EQ(openFlagsToString(O_TRUNC), "O_TRUNC");
  EXPECT_EQ(openFlagsToString(O_APPEND | O_CREAT), "O_APPEND | O_CREAT");
  EXPECT_EQ(openFlagsToString(O_APPEND | O_CREAT | O_DIRECT | O_EXCL),
            "O_APPEND | O_CREAT | O_DIRECT | O_EXCL");
}
