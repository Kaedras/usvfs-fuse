#pragma once

#include "fdmap.h"
#include <condition_variable>
#include <mutex>
#include <string>

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
  struct StatusData
  {
    Status status = unknown;
    std::condition_variable cv;
    std::mutex mtx;
  };
  std::string upperDir;
  std::string mountpoint;
  std::shared_ptr<VirtualFileTreeItem> fileTree;
  FdMap fdMap;
  fuse* fusePtr          = nullptr;
  StatusData* statusData = nullptr;
  bool debugMode         = false;
  bool useMountNamespace = false;

  char* stack    = nullptr;  // Start of stack buffer
  char* stackTop = nullptr;  // End of stack buffer
  int pidFd      = -1;
  int eventFd    = -1;
  int nsFd       = -1;
  uid_t uid      = -1;
  uid_t gid      = -1;

  ~MountState();
};
