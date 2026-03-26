#include "utils.h"

using namespace std;
using namespace icu;

bool iequals(const std::string_view lhs, const std::string_view rhs) noexcept
{
  if (lhs.length() != rhs.length()) {
    return false;
  }

  // try a fast ASCII comparison first
  bool isAscii = true;
  for (size_t i = 0; i < lhs.length(); ++i) {
    const auto a = static_cast<unsigned char>(lhs[i]);
    const auto b = static_cast<unsigned char>(rhs[i]);

    if (a > 127 || b > 127) {
      isAscii = false;
      break;
    }
    if (tolower(a) != tolower(b)) {
      return false;
    }
  }

  if (isAscii) {
    return true;
  }
  const auto a = UnicodeString::fromUTF8(lhs);
  const auto b = UnicodeString::fromUTF8(rhs);

  return a.caseCompare(b, 0) == 0;
}

bool iendsWith(const std::string_view lhs, const std::string_view rhs) noexcept
{
  if (rhs.empty()) {
    return true;
  }
  if (lhs.length() < rhs.length()) {
    return false;
  }

  const auto suffix = lhs.substr(lhs.length() - rhs.length());
  return iequals(suffix, rhs);
}

bool istartsWith(const std::string_view lhs, const std::string_view rhs) noexcept
{
  if (rhs.empty()) {
    return true;
  }
  if (lhs.length() < rhs.length()) {
    return false;
  }

  const auto prefix = lhs.substr(0, rhs.length());
  return iequals(prefix, rhs);
}

std::string toLower(const std::string_view str) noexcept
{
  // try using ASCII first
  bool isAscii = true;
  for (const unsigned char c : str) {
    if (c > 127) {
      isAscii = false;
      break;
    }
  }

  string result;

  if (isAscii) {
    result.reserve(str.length());
    for (const char c : str) {
      result.push_back(static_cast<char>(tolower(c)));
    }
    return result;
  }

  auto unicodeStr = UnicodeString::fromUTF8(str);
  unicodeStr.toLower();

  unicodeStr.toUTF8String(result);
  return result;
}

void toLowerInplace(std::string& str) noexcept
{
  // try using ASCII first
  bool isAscii = true;
  for (const unsigned char c : str) {
    if (c > 127) {
      isAscii = false;
      break;
    }
  }

  if (isAscii) {
    for (char& c : str) {
      c = static_cast<char>(tolower(c));
    }
    return;
  }

  auto unicodeStr = UnicodeString::fromUTF8(str);
  unicodeStr.toLower();

  str.clear();
  unicodeStr.toUTF8String(str);
}

std::string toUpper(const std::string_view str) noexcept
{
  // try using ASCII first
  bool isAscii = true;
  for (const unsigned char c : str) {
    if (c > 127) {
      isAscii = false;
      break;
    }
  }

  string result;

  if (isAscii) {
    result.reserve(str.length());
    for (const char c : str) {
      result.push_back(static_cast<char>(toupper(c)));
    }
    return result;
  }

  auto unicodeStr = UnicodeString::fromUTF8(str);
  unicodeStr.toUpper();

  unicodeStr.toUTF8String(result);
  return result;
}

void toUpperInplace(std::string& str) noexcept
{
  // try using ASCII first
  bool isAscii = true;
  for (const unsigned char c : str) {
    if (c > 127) {
      isAscii = false;
      break;
    }
  }

  if (isAscii) {
    for (char& c : str) {
      c = static_cast<char>(toupper(c));
    }
    return;
  }

  auto unicodeStr = UnicodeString::fromUTF8(str);
  unicodeStr.toUpper();

  str.clear();
  unicodeStr.toUTF8String(str);
}

std::string getFileNameFromPath(const std::string_view path) noexcept
{
  const size_t pos = path.find_last_of('/');
  if (pos == string::npos) {
    return string(path);
  }
  return string(path.substr(pos + 1));
}

std::string getParentPath(const std::string_view path) noexcept
{
  const size_t pos = path.find_last_of('/');
  if (pos == string::npos) {
    return string(path);
  }
  return string(path.substr(0, pos));
}

bool isParentPathOf(std::string_view parentPath, std::string_view path) noexcept
{
  if (parentPath.empty() || parentPath == "/") {
    return true;
  }

  if (parentPath.ends_with('/')) {
    parentPath.remove_suffix(1);
  }
  if (path.ends_with('/')) {
    path.remove_suffix(1);
  }

  if (parentPath.length() > path.length()) {
    return false;
  }
  if (parentPath.length() == path.length()) {
    return iequals(parentPath, path);
  }

  try {
    return istartsWith(path, parentPath) && path.at(parentPath.length()) == '/';
  } catch (const out_of_range&) {
    return false;
  }
}

std::string_view relativePath(std::string_view path, std::string_view base) noexcept
{
  if (path == base) {
    return {};
  }
  // remove trailing slashes
  if (path.ends_with('/')) {
    path.remove_suffix(1);
  }
  if (base.ends_with('/')) {
    base.remove_suffix(1);
  }

  string_view relative = path;
  relative.remove_prefix(base.size() + 1);

  return relative;
}

string openFlagsToString(int flags) noexcept
{
#define CHECK_FLAG(flag)                                                               \
  if (flags & flag) {                                                                  \
    flagStrings.emplace_back(#flag);                                                   \
  }

  static constexpr int maxFlagCount = 18;

  vector<string> flagStrings;
  flagStrings.reserve(maxFlagCount);

  CHECK_FLAG(O_APPEND)
  CHECK_FLAG(O_ASYNC)
  CHECK_FLAG(O_CLOEXEC)
  CHECK_FLAG(O_CREAT)
  CHECK_FLAG(O_DIRECT)
  CHECK_FLAG(O_DIRECTORY)
  CHECK_FLAG(O_DSYNC)
  CHECK_FLAG(O_EXCL)
  CHECK_FLAG(O_LARGEFILE)
  CHECK_FLAG(O_NOATIME)
  CHECK_FLAG(O_NOCTTY)
  CHECK_FLAG(O_NOFOLLOW)
  CHECK_FLAG(O_NONBLOCK)
  CHECK_FLAG(O_NDELAY)
  CHECK_FLAG(O_PATH)
  CHECK_FLAG(O_SYNC)
  CHECK_FLAG(O_TMPFILE)
  CHECK_FLAG(O_TRUNC)

  if (flagStrings.empty()) {
    return {};
  }

  if (flagStrings.size() == 1) {
    return flagStrings.front();
  }

  string result;
  for (size_t i = 0; i < flagStrings.size() - 1; ++i) {
    result += flagStrings.at(i) + " | ";
  }
  result += flagStrings.at(flagStrings.size() - 1);

  return result;
#undef CHECK_FLAG
}

std::vector<std::string_view> createEnv() noexcept
{
  // determine vector size
  // iterating over environ twice and allocating all memory at once is faster than
  // iterating once and allocating memory for each item separately
  int count = 0;
  while (environ[count] != nullptr) {
    ++count;
  }

  vector<string_view> env;
  env.reserve(count);

  for (int i = 0; environ[i] != nullptr; ++i) {
    env.emplace_back(environ[i]);
  }
  return env;
}
