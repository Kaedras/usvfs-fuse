#include "usvfs-fuse/fdmap.h"

#include "usvfs-fuse/logger.h"
#include "usvfs-fuse/utils.h"

int FdMap::at(const std::string_view path) const noexcept
{
  try {
    return map.at(toLower(path));
  } catch (const std::out_of_range&) {
    logger::error("error geting dirFd for '{}'", path);
    return -1;
  }
}

int& FdMap::operator[](const std::string_view path) noexcept
{
  return map[toLower(path)];
}

std::unordered_map<std::string, int>::iterator FdMap::begin() noexcept
{
  return map.begin();
}

std::unordered_map<std::string, int>::iterator FdMap::end() noexcept
{
  return map.end();
}
