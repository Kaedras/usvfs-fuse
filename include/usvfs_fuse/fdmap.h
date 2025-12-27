#pragma once

// wrapper class for std::unordered_map that returns an invalid file descriptor instead
// of throwing std::out_of_range and converts keys to lower case
class FdMap
{
public:
  int at(const std::string& path) const noexcept;
  int& operator[](const std::string& path) noexcept;

  std::unordered_map<std::string, int>::iterator begin() noexcept;
  std::unordered_map<std::string, int>::iterator end() noexcept;

private:
  std::unordered_map<std::string, int> map;
};
