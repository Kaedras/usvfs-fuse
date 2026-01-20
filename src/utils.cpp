#include "utils.h"

using namespace std;
using namespace icu;

bool iequals(const std::string_view lhs, const std::string_view rhs) noexcept
{
  if (lhs.length() != rhs.length()) {
    return false;
  }

  // try a fast ASCII comparison first
  bool is_ascii = true;
  for (size_t i = 0; i < lhs.length(); ++i) {
    const auto a = static_cast<unsigned char>(lhs[i]);
    const auto b = static_cast<unsigned char>(rhs[i]);

    if (a > 127 || b > 127) {
      is_ascii = false;
      break;
    }
    if (tolower(a) != tolower(b)) {
      return false;
    }
  }

  if (is_ascii) {
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
  bool is_ascii = true;
  for (const unsigned char c : str) {
    if (c > 127) {
      is_ascii = false;
      break;
    }
  }

  string result;
  result.reserve(str.length());

  if (is_ascii) {
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
  bool is_ascii = true;
  for (const unsigned char c : str) {
    if (c > 127) {
      is_ascii = false;
      break;
    }
  }

  if (is_ascii) {
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
  bool is_ascii = true;
  for (const unsigned char c : str) {
    if (c > 127) {
      is_ascii = false;
      break;
    }
  }

  string result;
  result.reserve(str.length());

  if (is_ascii) {
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
  bool is_ascii = true;
  for (const unsigned char c : str) {
    if (c > 127) {
      is_ascii = false;
      break;
    }
  }

  if (is_ascii) {
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
