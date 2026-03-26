#include "mountstate.h"

MountState::~MountState()
{
  spdlog::trace("deleting status data");
  if (useMountNamespace) {
    if (munmap(statusData, sizeof(StatusData)) == -1) {
      spdlog::error("munmap error: {}", strerror(errno));
    }
  } else {
    delete statusData;
  }
}
