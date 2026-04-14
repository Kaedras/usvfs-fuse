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

  int& operator[](std::string_view path) noexcept;

  /**
   * @brief Get a file descriptor from the map
   */
  int at(std::string_view path, bool doNotLogOnFail = false) const noexcept;

  /**
   * @brief Get a file descriptor from the map without converting the path to lower-case
   */
  int atLc(std::string_view lcPath, bool doNotLogOnFail = false) const noexcept;

  /**
   * @brief Add a file descriptor to the map
   */
  bool add(std::string_view path, int fd) noexcept;

  /**
   * @brief Add a file descriptor to the map without converting the path to lower-case
   */
  bool addLc(std::string_view lcPath, int fd) noexcept;

  /**
   * @brief Merge another fd map into this one and invalidate other file descriptors so
   * they are not closed on destruction
   */
  void merge(FdMap& other) noexcept;

  std::unordered_map<std::string, int>::iterator begin() noexcept;
  std::unordered_map<std::string, int>::iterator end() noexcept;

private:
  // from https://en.cppreference.com/w/cpp/container/unordered_map/find.html
  struct string_hash
  {
    using hash_type      = std::hash<std::string_view>;
    using is_transparent = void;

    std::size_t operator()(const char* str) const { return hash_type{}(str); }
    std::size_t operator()(const std::string_view str) const
    {
      return hash_type{}(str);
    }
    std::size_t operator()(const std::string& str) const { return hash_type{}(str); }
  };

  std::unordered_map<std::string, int, string_hash, std::equal_to<>> m_map;
};
