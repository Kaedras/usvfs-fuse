#pragma once

#include <string>

struct MountData
{
  // dtor closes shutdownEventFd, so we have to implement moving and disallow copying
  MountData(int pidFd, int shutdownEventFd, int dumpEventFd, int dumpPipeFd,
            char* stack, std::string mountpoint) noexcept;

  MountData(MountData&) = delete;
  MountData(MountData&&) noexcept;
  MountData& operator=(MountData&) = delete;
  MountData& operator=(MountData&& other) noexcept;

  ~MountData();

  int pidFd           = -1;
  int shutdownEventFd = -1;
  int dumpEventFd     = -1;
  int dumpPipeFd      = -1;
  char* stack         = nullptr;
  std::string mountpoint;
};
