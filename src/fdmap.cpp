#include "fdmap.h"

#include <cstring>
#include <fcntl.h>
#include <ranges>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include "utils.h"

using namespace std;

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

int& FdMap::operator[](const std::string_view path) noexcept
{
  return m_map[toLower(path)];
}

int FdMap::at(const std::string_view path, const bool doNotLogOnFail) const noexcept
{
  const auto result = m_map.find(toLower(path));
  if (result != m_map.end()) {
    return result->second;
  }

  if (!doNotLogOnFail) {
    spdlog::error("error getting dirFd for '{}'", path);
  }
  return -1;
}

int FdMap::atLc(std::string_view lcPath, bool doNotLogOnFail) const noexcept
{
  const auto result = m_map.find(lcPath);
  if (result != m_map.end()) {
    return result->second;
  }

  if (!doNotLogOnFail) {
    spdlog::error("error getting dirFd for '{}'", lcPath);
  }
  return -1;
}

bool FdMap::add(const std::string_view path, int fd) noexcept
{
  return m_map.emplace(toLower(path), fd).second;
}

bool FdMap::addLc(std::string_view lcPath, int fd) noexcept
{
  return m_map.emplace(lcPath, fd).second;
}

int FdMap::add(const std::string& path) noexcept
{
  string pathLc = toLower(path);
  const auto it = m_map.find(pathLc);
  if (it != m_map.end()) {
    return it->second;
  }

  int fd = open(path.c_str(), O_DIRECTORY | O_PATH);
  if (fd == -1) {
    spdlog::error("error opening directory {}: {}", path, strerror(errno));
    return -1;
  }
  if (!m_map.emplace(std::move(pathLc), fd).second) {
    spdlog::error("error adding fd for '{}'", path);
    close(fd);
    return -1;
  }

  spdlog::trace("added fd {} for '{}'", fd, path);
  return fd;
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
