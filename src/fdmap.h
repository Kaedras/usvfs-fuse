#pragma once

// wrapper class for std::unordered_map that returns an invalid file descriptor instead
// of throwing std::out_of_range and converts keys to lower case
class FdMap
{
public:
  int at(std::string_view path) const noexcept;
  int& operator[](std::string_view path) noexcept;

  std::unordered_map<std::string, int>::iterator begin() noexcept;
  std::unordered_map<std::string, int>::iterator end() noexcept;

private:
  std::unordered_map<std::string, int> map;
};
