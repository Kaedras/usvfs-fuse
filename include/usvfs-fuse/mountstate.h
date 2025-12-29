#pragma once

#include "fdmap.h"

struct fuse;
class VirtualFileTreeItem;

struct MountState
{
  std::string upperDir;
  std::string mountpoint;
  std::shared_ptr<VirtualFileTreeItem> fileTree;
  FdMap fdMap;
  fuse* fusePtr = nullptr;

  char* stack    = nullptr;  // Start of stack buffer
  char* stackTop = nullptr;  // End of stack buffer
  int pidFd      = -1;
  int nsFd       = -1;
  uid_t uid;
  uid_t gid;

  ~MountState();
};
