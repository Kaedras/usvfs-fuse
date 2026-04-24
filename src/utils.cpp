#include "utils.h"

using namespace std;

bool iequals(const std::string_view lhs, const std::string_view rhs) noexcept
{
  if (lhs.length() != rhs.length()) {
    return false;
  }

  // try a fast ASCII comparison first
  bool isAscii = true;
  for (size_t i = 0; i < lhs.length(); ++i) {
    const char a = lhs[i];
    const char b = rhs[i];

    if (!isascii(a) || !isascii(b)) {
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

  const auto a = QString::fromUtf8(lhs);
  const auto b = QString::fromUtf8(rhs);

  return a.compare(b, Qt::CaseInsensitive) == 0;
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
  for (const char c : str) {
    if (!isascii(c)) {
      isAscii = false;
      break;
    }
  }

  if (isAscii) {
    string result;
    result.reserve(str.length());
    for (const char c : str) {
      result.push_back(static_cast<char>(tolower(c)));
    }
    return result;
  }

  QString qstr = QString::fromLocal8Bit(str);
  for (auto& c : qstr) {
    c = c.toLower();
  }

  return qstr.toStdString();
}

void toLowerInplace(std::string& str) noexcept
{
  // try using ASCII first
  bool isAscii = true;
  for (const char c : str) {
    if (!isascii(c)) {
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

  QString qstr = QString::fromLocal8Bit(str);
  for (auto& c : qstr) {
    c = c.toLower();
  }
  str = qstr.toStdString();
}

std::string toUpper(const std::string_view str) noexcept
{
  // try using ASCII first
  bool isAscii = true;
  for (const char c : str) {
    if (!isascii(c)) {
      isAscii = false;
      break;
    }
  }

  if (isAscii) {
    string result;
    result.reserve(str.length());
    for (const char c : str) {
      result.push_back(static_cast<char>(toupper(c)));
    }
    return result;
  }

  QString qstr = QString::fromLocal8Bit(str);
  for (auto& c : qstr) {
    c = c.toUpper();
  }

  return qstr.toStdString();
}

void toUpperInplace(std::string& str) noexcept
{
  // try using ASCII first
  bool isAscii = true;
  for (const char c : str) {
    if (!isascii(c)) {
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

  QString qstr = QString::fromLocal8Bit(str);
  for (auto& c : qstr) {
    c = c.toUpper();
  }
  str = qstr.toStdString();
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

std::string_view relativePath(std::string_view p, std::string_view base) noexcept
{
  if (p == base) {
    return {};
  }
  // remove trailing slashes
  if (p.ends_with('/')) {
    p.remove_suffix(1);
  }
  if (base.ends_with('/')) {
    base.remove_suffix(1);
  }

  string_view relative = p;
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

void maximizeFdLimit()
{
  rlimit lim{};
  auto res = getrlimit(RLIMIT_NOFILE, &lim);
  if (res != 0) {
    spdlog::warn("getrlimit() failed with {}", strerror(errno));
    return;
  }
  lim.rlim_cur = lim.rlim_max;
  res          = setrlimit(RLIMIT_NOFILE, &lim);
  if (res != 0) {
    spdlog::warn("setrlimit() failed with {}", strerror(errno));
  }
}
