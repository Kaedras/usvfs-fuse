#include "mountstate.h"

#include "logger.h"

MountState::~MountState()
{
  logger::trace("deleting status data");
  if (useMountNamespace) {
    if (munmap(statusData, sizeof(StatusData)) == -1) {
      logger::error("munmap error: {}", strerror(errno));
    }
  } else {
    delete statusData;
  }
}
