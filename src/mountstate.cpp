#include "mountstate.h"

#include "logger.h"

MountState::~MountState()
{
  for (const auto& fd : fdMap | std::views::values) {
    logger::trace("closing fd {}", fd);
    close(fd);
  }
}
