#include "mountstate.h"

MountState::~MountState()
{
  spdlog::trace("cleaning up mount state");
  // only close status event fd here, other event fds are closed elsewhere
  if (statusEventFd != -1) {
    close(statusEventFd);
  }
}
