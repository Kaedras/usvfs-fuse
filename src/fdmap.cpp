#include "fdmap.h"

#include "utils.h"

FdMap::FdMap() noexcept = default;

FdMap::FdMap(FdMap&& other) noexcept
{
  m_map = std::move(other.m_map);
}

FdMap& FdMap::operator=(FdMap&& other) noexcept
{
  m_map = std::move(other.m_map);
  return *this;
}

FdMap::~FdMap() noexcept
{
  for (const auto& fd : m_map | std::views::values) {
    if (fd >= 0) {
      spdlog::trace("closing fd {}", fd);
      close(fd);
    }
  }
}

int FdMap::at(const std::string_view path) const noexcept
{
  try {
    return m_map.at(toLower(path));
  } catch (const std::out_of_range&) {
    spdlog::error("error geting dirFd for '{}'", path);
    return -1;
  }
}

int& FdMap::operator[](const std::string_view path) noexcept
{
  return m_map[toLower(path)];
}

void FdMap::merge(FdMap& other) noexcept
{
  for (auto& [path, fd] : other.m_map) {
    m_map[path] = fd;
    fd          = -1;
  }
}

std::unordered_map<std::string, int>::iterator FdMap::begin() noexcept
{
  return m_map.begin();
}

std::unordered_map<std::string, int>::iterator FdMap::end() noexcept
{
  return m_map.end();
}
