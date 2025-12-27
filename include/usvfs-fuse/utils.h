#pragma once

#include <string>
#include <string_view>

bool iequals(std::string_view lhs, std::string_view rhs);
bool iendsWith(std::string_view lhs, std::string_view rhs);
bool istartsWith(std::string_view lhs, std::string_view rhs);
std::string toLower(std::string_view str);
std::string toUpper(std::string_view str);
void toUpperInplace(std::string& str);
void toLowerInplace(std::string& str);

std::string getFileNameFromPath(std::string_view path);

/**
 * @brief Returns the parent path of the provided path.
 * @note Returns empty string instead of "/"
 */
std::string getParentPath(std::string_view path);
