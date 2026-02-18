#include "mountstate.h"

#include "logger.h"

MountState::~MountState()
{
  for (const auto& fd : fdMap | std::views::values) {
    logger::trace("closing fd {}", fd);
    close(fd);
  }

  logger::trace("deleting status data");
  if (useMountNamespace) {
    if (munmap(statusData, sizeof(StatusData)) == -1) {
      logger::error("munmap error: {}", strerror(errno));
    }
  } else {
    delete statusData;
  }
}
