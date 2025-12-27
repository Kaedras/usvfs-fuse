#include "usvfs_fuse/mountstate.h"

#include "usvfs_fuse/logger.h"

MountState::~MountState()
{
  for (const auto& fd : fdMap | std::views::values) {
    logger::trace("closing fd {}", fd);
    close(fd);
  }
}
