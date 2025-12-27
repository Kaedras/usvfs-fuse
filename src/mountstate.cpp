#include "usvfs-fuse/mountstate.h"

#include "usvfs-fuse/logger.h"

MountState::~MountState()
{
  for (const auto& fd : fdMap | std::views::values) {
    logger::trace("closing fd {}", fd);
    close(fd);
  }
}
