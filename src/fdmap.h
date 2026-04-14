#pragma once

// wrapper class for std::unordered_map that returns an invalid file descriptor instead
// of throwing std::out_of_range, converts keys to lower case, and closes file
// descriptors on destruction
class FdMap
{
public:
  FdMap() noexcept;
  FdMap(FdMap&& other) noexcept;
  FdMap& operator=(FdMap&& other) noexcept;
  ~FdMap() noexcept;

  FdMap(const FdMap&)            = delete;
  FdMap& operator=(const FdMap&) = delete;

  int at(std::string_view path, bool doNotLogOnFail = false) const noexcept;
  int& operator[](std::string_view path) noexcept;
  /**
   * @brief Merge another fd map into this one and invalidate other file descriptors so
   * they are not closed on destruction
   */
  void merge(FdMap& other) noexcept;

  std::unordered_map<std::string, int>::iterator begin() noexcept;
  std::unordered_map<std::string, int>::iterator end() noexcept;

private:
  std::unordered_map<std::string, int> m_map;
};
