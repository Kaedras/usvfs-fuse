#include "mountstate.h"

#include <spdlog/spdlog.h>
#include <sys/mman.h>

MountState::~MountState()
{
  spdlog::trace("deleting status data");
  if (useMountNamespace) {
    if (munmap(statusData, sizeof(StatusData)) == -1) {
      spdlog::error("munmap error: {}", strerror(errno));
    }
    if (munmap(fusePtr, sizeof(fuse*)) == -1) {
      spdlog::error("munmap error: {}", strerror(errno));
    }
  } else {
    delete statusData;
    delete fusePtr;
  }
}
