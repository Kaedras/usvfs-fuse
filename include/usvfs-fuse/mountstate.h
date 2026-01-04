#pragma once

#include "fdmap.h"

struct fuse;
class VirtualFileTreeItem;

struct MountState
{
  enum Status
  {
    unknown,
    success,
    failure
  };
  std::string upperDir;
  std::string mountpoint;
  std::shared_ptr<VirtualFileTreeItem> fileTree;
  FdMap fdMap;
  fuse* fusePtr = nullptr;
  Status status = unknown;
  std::condition_variable cv;
  std::mutex mtx;

  char* stack    = nullptr;  // Start of stack buffer
  char* stackTop = nullptr;  // End of stack buffer
  int pidFd      = -1;
  int nsFd       = -1;
  uid_t uid;
  uid_t gid;

  ~MountState();
};
