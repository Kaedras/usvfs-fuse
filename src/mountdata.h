#pragma once

struct MountData
{
  // dtor closes closeEventFd, so we have to implement moving and disallow copying
  MountData(int pidFd, int closeEventFd, int dumpEventFd, int dumpPipeFd, char* stack,
            std::string mountpoint) noexcept;

  MountData(MountData&) = delete;
  MountData(MountData&&) noexcept;
  MountData& operator=(MountData&) = delete;
  MountData& operator=(MountData&& other) noexcept;

  ~MountData();

  int pidFd        = -1;
  int closeEventFd = -1;
  int dumpEventFd  = -1;
  int dumpPipeFd   = -1;
  char* stack      = nullptr;
  std::string mountpoint;
};
