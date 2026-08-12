#include "mountstate.h"

#include <spdlog/spdlog.h>
#include <sys/mman.h>

MountState::~MountState()
{
  spdlog::trace("cleaning up shared memory");
  if (munmap(statusData, sizeof(StatusData)) == -1) {
    spdlog::error("munmap error: {}", strerror(errno));
  }
  if (munmap(fusePtr, sizeof(fuse*)) == -1) {
    spdlog::error("munmap error: {}", strerror(errno));
  }
}
