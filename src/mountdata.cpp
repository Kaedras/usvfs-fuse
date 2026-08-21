#include "mountdata.h"

MountData::MountData(int pidFd, int shutdownEventFd, int dumpEventFd, int dumpPipeFd,
                     char* stack, std::string mountpoint) noexcept
    : pidFd(pidFd), shutdownEventFd(shutdownEventFd), dumpEventFd(dumpEventFd),
      dumpPipeFd(dumpPipeFd), stack(stack), mountpoint(std::move(mountpoint))
{}

MountData::MountData(MountData&& other) noexcept
{
  pidFd           = other.pidFd;
  shutdownEventFd = other.shutdownEventFd;
  dumpEventFd     = other.dumpEventFd;
  dumpPipeFd      = other.dumpPipeFd;
  stack           = other.stack;
  mountpoint      = other.mountpoint;

  // invalidate other fd to prevent closing it in dtor
  other.shutdownEventFd = -1;
  other.dumpEventFd     = -1;
  other.dumpPipeFd      = -1;
}

MountData& MountData::operator=(MountData&& other) noexcept
{
  pidFd           = other.pidFd;
  shutdownEventFd = other.shutdownEventFd;
  dumpEventFd     = other.dumpEventFd;
  dumpPipeFd      = other.dumpPipeFd;
  stack           = other.stack;
  mountpoint      = other.mountpoint;

  // invalidate other fd to prevent closing it in dtor
  other.shutdownEventFd = -1;
  other.dumpEventFd     = -1;
  other.dumpPipeFd      = -1;

  return *this;
}

MountData::~MountData()
{
  if (shutdownEventFd != -1) {
    close(shutdownEventFd);
  }
  if (dumpEventFd != -1) {
    close(dumpEventFd);
  }
  if (dumpPipeFd != -1) {
    close(dumpPipeFd);
  }
  if (stack != nullptr) {
    if (munmap(stack, sizeof(stack)) == -1) {
      spdlog::error("munmap error: {}", strerror(errno));
    }
  }
}
