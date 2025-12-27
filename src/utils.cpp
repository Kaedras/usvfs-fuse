#include "usvfs_fuse/utils.h"

using namespace std;
using namespace icu;

bool iequals(const std::string_view lhs, const std::string_view rhs)
{
  if (lhs.length() != rhs.length()) {
    return false;
  }

  // try a fast ASCII comparison first
  bool is_ascii = true;
  for (size_t i = 0; i < lhs.length(); ++i) {
    if (static_cast<unsigned char>(lhs[i]) > 127 ||
        static_cast<unsigned char>(rhs[i]) > 127) {
      is_ascii = false;
      break;
    }
    if (std::tolower(static_cast<unsigned char>(lhs[i])) !=
        std::tolower(static_cast<unsigned char>(rhs[i]))) {
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

bool iendsWith(const std::string_view lhs, const std::string_view rhs)
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

bool istartsWith(const std::string_view lhs, const std::string_view rhs)
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

std::string toLower(const std::string_view str)
{
  auto unicodeStr = UnicodeString::fromUTF8(str);
  unicodeStr.toLower();

  string result;
  unicodeStr.toUTF8String(result);
  return result;
}

void toLowerInplace(std::string& str)
{
  auto unicodeStr = UnicodeString::fromUTF8(str);
  unicodeStr.toLower();

  str.clear();
  unicodeStr.toUTF8String(str);
}

std::string toUpper(const std::string_view str)
{
  auto unicodeStr = UnicodeString::fromUTF8(str);
  unicodeStr.toUpper();

  string result;
  unicodeStr.toUTF8String(result);
  return result;
}

void toUpperInplace(std::string& str)
{
  auto unicodeStr = UnicodeString::fromUTF8(str);
  unicodeStr.toUpper();

  str.clear();
  unicodeStr.toUTF8String(str);
}

std::string getFileNameFromPath(const std::string_view path)
{
  const size_t pos = path.find_last_of('/');
  if (pos == std::string::npos) {
    return string(path);
  }
  return string(path.substr(pos + 1));
}

std::string getParentPath(const std::string_view path)
{
  const size_t pos = path.find_last_of('/');
  if (pos == std::string::npos) {
    return string(path);
  }
  return string(path.substr(0, pos));
}
