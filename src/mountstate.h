#pragma once

#include "fdmap.h"
#include <memory>
#include <string>

struct fuse;
class VirtualFileTreeItem;

struct MountState
{
  std::string upperDir;
  std::string mountpoint;
  std::shared_ptr<VirtualFileTreeItem> fileTree;
  FdMap fdMap;
  fuse* fusePtr          = nullptr;
  bool debugMode         = false;
  bool useMountNamespace = false;

  int pidFd           = -1;
  int statusEventFd   = -1;
  int shutdownEventFd = -1;
  int dumpEventFd     = -1;
  int dumpPipeFd      = -1;
  int nsFd            = -1;
  uid_t uid           = -1;
  uid_t gid           = -1;

  ~MountState();
};
