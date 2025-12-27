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

  ~MountState();
};
