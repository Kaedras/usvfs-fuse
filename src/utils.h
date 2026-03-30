#pragma once

#include <string>
#include <string_view>

bool iequals(std::string_view lhs, std::string_view rhs) noexcept;
bool iendsWith(std::string_view lhs, std::string_view rhs) noexcept;
bool istartsWith(std::string_view lhs, std::string_view rhs) noexcept;
std::string toLower(std::string_view str) noexcept;
std::string toUpper(std::string_view str) noexcept;
void toUpperInplace(std::string& str) noexcept;
void toLowerInplace(std::string& str) noexcept;

/**
 * @brief Extracts the file name from a given path.
 **/
std::string getFileNameFromPath(std::string_view path) noexcept;

/**
 * @brief Returns the parent path of the provided path.
 * @note Returns empty string instead of "/".
 */
std::string getParentPath(std::string_view path) noexcept;

/**
 * @brief Checks if `parentPath` is a parent directory of `path`.
 */
bool isParentPathOf(std::string_view parentPath, std::string_view path) noexcept;

/**
  @brief Functions similar to std::filesystem::relative, but does not access the
filesystem.
  @note Does not check for errors and assumes that `p` is a subdirectory of `base`.
**/
std::string_view relativePath(std::string_view p, std::string_view base) noexcept;

/**
 * @brief Creates a string to describe the flags passed to `open()`.
 */
std::string openFlagsToString(int flags) noexcept;
