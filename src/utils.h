#pragma once

#include <string>
#include <string_view>
#include <vector>

bool iequals(std::string_view lhs, std::string_view rhs) noexcept;
bool iendsWith(std::string_view lhs, std::string_view rhs) noexcept;
bool istartsWith(std::string_view lhs, std::string_view rhs) noexcept;
std::string toLower(std::string_view str) noexcept;
std::string toUpper(std::string_view str) noexcept;
void toUpperInplace(std::string& str) noexcept;
void toLowerInplace(std::string& str) noexcept;

std::string getFileNameFromPath(std::string_view path) noexcept;

/**
 * @brief Returns the parent path of the provided path.
 * @note Returns empty string instead of "/"
 */
std::string getParentPath(std::string_view path) noexcept;

bool isParentPathOf(std::string_view parentPath, std::string_view path) noexcept;

// functions similar to std::filesystem::relative, but without accessing the filesystem
// does not check for errors and assumes that `p` is a subdirectory of `base`
std::string_view relativePath(std::string_view p, std::string_view base) noexcept;

std::vector<std::string_view> createEnv() noexcept;
