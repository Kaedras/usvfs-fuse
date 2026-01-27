#include "usvfs.h"

#include "logger.h"
#include "mountstate.h"
#include "utils.h"
#include "virtualfiletreeitem.h"

using namespace std;
namespace fs = std::filesystem;

#define GET_STATE()                                                                    \
  auto* state = getState();                                                            \
  if (state == nullptr) {                                                              \
    logger::error("error getting state");                                              \
    return -EIO;                                                                       \
  }

#define FIND_ITEM()                                                                    \
  auto item = state->fileTree->find(path);                                             \
  if (item == nullptr) {                                                               \
    return -ENOENT;                                                                    \
  }

namespace
{
MountState* getState()
{
  const auto* context = fuse_get_context();
  return static_cast<MountState*>(context ? context->private_data : nullptr);
}

int createParentDir(MountState* state, string_view realParentPath, string_view fileName,
                    mode_t mode)
{
  // create parent directory
  string parentName = getFileNameFromPath(realParentPath);
  int grandParentFd = state->fdMap.at(getParentPath(realParentPath));
  logger::trace("creating parent directory {}", getParentPath(realParentPath));
  if (mkdirat(grandParentFd, parentName.c_str(), mode) == -1) {
    const int e = errno;
    logger::error("error creating parent directory, mkdirat failed: {}", realParentPath,
                  fileName, strerror(e));
    return -e;
  }

  // open parent directory
  int parentFd = openat(grandParentFd, parentName.c_str(), OPEN_FLAGS);
  if (parentFd == -1) {
    const int e = errno;
    logger::error("error opening parent directory '{}': {}", realParentPath,
                  strerror(e));
    return -e;
  }

  // insert fd into fd map
  logger::trace("adding fd {} for '{}'", parentFd, realParentPath);
  state->fdMap[realParentPath] = parentFd;

  return parentFd;
}
}  // namespace

int usvfs_getattr(const char* path, struct stat* stbuf, fuse_file_info* fi) noexcept
{
  logger::trace("usvfs_getattr(path={})", path);

  // try to use existing fd
  if (fi != nullptr && fi->fh > 0) {
    if (fstat(static_cast<int>(fi->fh), stbuf) == -1) {
      const int e = errno;
      logger::error("fstat failed: {}", strerror(e));
      return -e;
    }
    return 0;
  }

  GET_STATE()

  string pathToUse = path;

  static constexpr string directorySuffix       = "/.directory";
  static constexpr size_t directorySuffixLength = directorySuffix.length() - 1;

  if (pathToUse.ends_with(directorySuffix)) {
    pathToUse.erase(pathToUse.size() - directorySuffixLength);
  }

  const auto item = state->fileTree->find(pathToUse);

  if (item == nullptr) {
    return -ENOENT;
  }

  int res;
  int fd;
  string fdPath;
  string file;
  if (item->isDir()) {
    fdPath = item->realPath();
    fd     = state->fdMap.at(fdPath);
    res    = fstatat(fd, "", stbuf, AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH);
  } else {
    fdPath = getParentPath(item->realPath());
    file   = getFileNameFromPath(item->realPath());
    fd     = state->fdMap.at(fdPath);
    res    = fstatat(fd, file.c_str(), stbuf, AT_SYMLINK_NOFOLLOW);
  }

  if (res == -1) {
    const int e = errno;
    logger::error("usvfs_getattr: fstatat(fd={}:'{}', file='{}') failed: {}", fd,
                  fdPath, file, strerror(e));
    return -e;
  }

  return 0;
}

int usvfs_readlink(const char* path, char* buf, size_t size) noexcept
{
  logger::trace("usvfs_readlink(path='{}',buf={},size={})", path,
                reinterpret_cast<long>(buf), size);
  GET_STATE()
  FIND_ITEM()

  ssize_t res;
  if (item->isDir()) {
    const string dir = item->realPath();
    res              = readlinkat(state->fdMap.at(dir), "", buf, size);
  } else {
    const string dir = item->getParent().lock()->realPath();
    res = readlinkat(state->fdMap.at(dir), item->fileName().c_str(), buf, size);
  }
  if (res == -1) {
    return -errno;
  }
  buf[res] = '\0';
  return 0;
}

int usvfs_mkdir(const char* path, mode_t mode) noexcept
{
  logger::trace("usvfs_mkdir(path='{}', mode={})", path, mode);
  GET_STATE()

  const string fileName = getFileNameFromPath(path);

  // check for existing items
  const auto existing = state->fileTree->find(path, true);
  if (existing != nullptr) {
    if (!existing->isDeleted()) {
      return -EEXIST;
    }
    // spdlog::get("hooks")->info("Rerouting file creation to original location of
    // deleted file: {}", existing->filePath());
    logger::info("Rerouting file creation to original location of deleted file: {}",
                 existing->filePath());
    existing->setDeleted(false);
    existing->setName(fileName);
    return 0;
  }

  // get parent item
  const auto parentItem = state->fileTree->find(getParentPath(path));
  if (parentItem == nullptr) {
    return -errno;
  }

  const string realParentPath = state->upperDir.empty()
                                    ? parentItem->realPath()
                                    : state->upperDir + parentItem->filePath();
  const string realPath       = realParentPath + "/" + fileName;

  logger::trace("usvfs_mkdir, path={}: creating directory in {}", path, realParentPath);

  // create the directory on disk
  int parentFd = state->fdMap.at(realParentPath);
  if (parentFd == -1 && !state->upperDir.empty()) {
    // parent path does not exist, this should only happen when upperDir is used
    parentFd = createParentDir(state, realParentPath, fileName, mode);
    if (parentFd < 0) {
      return parentFd;
    }
  }

  if (mkdirat(parentFd, fileName.c_str(), mode) < 0) {
    const int e = errno;
    logger::error("usvfs_mkdir: mkdirat failed: {}", realParentPath, fileName,
                  strerror(e));
    return -e;
  }

  // open and save the file descriptor
  int fd = openat(parentFd, fileName.c_str(), OPEN_FLAGS);
  if (fd < 0) {
    const int e = errno;
    logger::error("usvfs_mkdir: openat failed: {}", parentFd, realParentPath, fileName,
                  strerror(e));
    return -e;
  }
  logger::trace("adding fd {} for {}", fd, realPath);
  state->fdMap[realPath] = fd;

  // add the directory to the file tree
  const auto newItem = state->fileTree->add(path, realPath, dir);
  if (newItem == nullptr) {
    return -EIO;
  }

  return 0;
}

int usvfs_unlink(const char* path) noexcept
{
  logger::trace("usvfs_unlink(path='{}')", path);
  GET_STATE()
  FIND_ITEM()

  const string realParentPath = getParentPath(item->realPath());

  logger::trace("unlinkat {}, path: {}", realParentPath, item->fileName());

  if (unlinkat(state->fdMap.at(realParentPath), item->fileName().c_str(), 0) == -1) {
    const int e = errno;
    logger::error("unlink failed for '{}': {}", item->realPath(), strerror(e));
    return -e;
  }

  if (!state->fileTree->erase(path, false)) {
    return -errno;
  }
  return 0;
}

int usvfs_rmdir(const char* path) noexcept
{
  logger::trace("usvfs_rmdir(path='{}')", path);

  GET_STATE()
  FIND_ITEM()

  // check if the item is a directory
  if (!item->isDir()) {
    return -ENOTDIR;
  }

  // check if the directory is empty
  if (!item->isEmpty()) {
    return -ENOTEMPTY;
  }

  string realParentPath = getParentPath(item->realPath());

  if (unlinkat(state->fdMap.at(realParentPath), item->fileName().c_str(),
               AT_REMOVEDIR) == -1) {
    const int e = errno;
    logger::error("usvfs_rmdir: unlink failed for '{}': {}", item->realPath(),
                  strerror(e));
    return -e;
  }

  // mark the item as deleted
  item->setDeleted(true);

  return 0;
}

int usvfs_symlink(const char* target, const char* linkpath) noexcept
{
  logger::warn("usvfs_symlink(target='{}', linkpath='{}') - STUB!", target, linkpath);
  return -ENOSYS;
}

int usvfs_rename(const char* from, const char* to, const unsigned int flags) noexcept
{
  logger::trace("usvfs_rename(from='{}', to='{}', flags={})", from, to, flags);

  GET_STATE()

  // get old item
  const auto oldItem = state->fileTree->find(from);
  if (oldItem == nullptr) {
    logger::error("usvfs_rename: could not find item to rename");
    return -ENOENT;
  }

  // look for existing item
  if (state->fileTree->find(to) != nullptr && flags & RENAME_NOREPLACE) {
    logger::error("usvfs_rename: target path exists");
    return -EEXIST;
  }

  // create paths
  const string newParentPath = getParentPath(to);
  const auto newParentItem   = state->fileTree->find(newParentPath);
  if (newParentItem == nullptr) {
    logger::error("usvfs_rename: target parent directory '{}' does not exist",
                  newParentPath);
    return -ENOENT;
  }
  const string newRealParentPath = state->upperDir.empty()
                                       ? newParentItem->realPath()
                                       : state->upperDir + newParentItem->filePath();
  const string oldRealParentPath = getParentPath(oldItem->realPath());
  const string newFileName       = getFileNameFromPath(to);

  // rename on disk
  int oldFd = state->fdMap[oldRealParentPath];
  int newFd = state->fdMap[newRealParentPath];

  if (renameat2(oldFd, oldItem->fileName().c_str(), newFd, newFileName.c_str(),
                flags & RENAME_EXCHANGE ? RENAME_EXCHANGE : 0) != 0) {
    logger::error("usvfs_rename: renameat2({}:'{}', {}, {}:'{}', {}) failed: {}", oldFd,
                  oldRealParentPath, oldItem->fileName(), newFd, newRealParentPath,
                  newFileName, strerror(errno));
    return -errno;
  }

  // create new item
  const auto newItem =
      state->fileTree->add(to, newRealParentPath + to, oldItem->getType());
  if (newItem == nullptr) {
    logger::error("usvfs_rename: error inserting new path to file tree");
    return -errno;
  }

  // remove old item
  if (!state->fileTree->erase(from)) {
    logger::error("usvfs_rename: error removing '{}' from file tree", from);
    return -errno;
  }

  return 0;
}

int usvfs_link(const char* from, const char* to) noexcept
{
#warning STUB
  logger::warn("usvfs_link(from='{}', to='{}') - STUB!", from, to);
  return -ENOSYS;
}

int usvfs_chmod(const char* path, mode_t mode, fuse_file_info* fi) noexcept
{
  logger::trace("usvfs_chmod(path='{}', mode='{}')", path, mode);

  if (fi != nullptr && fi->fh != 0) {
    if (fchmod(static_cast<int>(fi->fh), mode) != 0) {
      const int e = errno;
      logger::error("usvfs_chmod: fchmod failed: {}", strerror(e));
      return -e;
    }
    return 0;
  }

  GET_STATE()
  FIND_ITEM()

  const string parentPath = getParentPath(item->realPath());
  int fd                  = state->fdMap.at(parentPath);
  if (fchmodat(fd, item->fileName().c_str(), mode, 0) == -1) {
    const int e = errno;
    logger::error("usvfs_chmod: fchmodat failed: {}", strerror(e));
    return -e;
  }
  return 0;
}

int usvfs_chown(const char* path, uid_t uid, gid_t gid, fuse_file_info* fi) noexcept
{
  logger::trace("usvfs_chown(path='{}', uid={}, gid={})", path, uid, gid);

  // try to use existing fd
  if (fi != nullptr && fi->fh > 0) {
    if (fchown(static_cast<int>(fi->fh), uid, gid) == -1) {
      const int e = errno;
      logger::error("usvfs_chown: fchown failed: {}", strerror(e));
      return -e;
    }
    return 0;
  }

  GET_STATE()
  FIND_ITEM()

  const string parentPath = getParentPath(item->realPath());
  if (fchownat(state->fdMap.at(parentPath), item->fileName().c_str(), uid, gid, 0) ==
      -1) {
    const int e = errno;
    logger::error("usvfs_chown: fchownat failed: {}", strerror(e));
    return -e;
  }
  return 0;
}

int usvfs_truncate(const char* path, off_t size, fuse_file_info* fi) noexcept
{
  logger::trace("usvfs_truncate(path='{}', size={})", path, size);

  // try to use existing fd
  if (fi != nullptr && fi->fh > 0) {
    if (ftruncate(static_cast<int>(fi->fh), size) == -1) {
      const int e = errno;
      logger::error("usvfs_truncate: ftruncate failed: {}", strerror(e));
      return -e;
    }
    return 0;
  }

  GET_STATE()
  FIND_ITEM()

  const string parentPath = getParentPath(item->realPath());

  const int parentFd = state->fdMap.at(parentPath);
  const int fd       = openat(parentFd, item->fileName().c_str(), O_WRONLY);
  if (fd == -1) {
    const int e = errno;
    logger::error("usvfs_truncate: openat({}:'{}', {}, O_WRONLY) failed: {}", parentFd,
                  parentPath, item->fileName(), strerror(e));
    return -e;
  }

  if (ftruncate(fd, size) < 0) {
    const int e = errno;
    logger::error("usvfs_truncate: ftruncate({}:'{}') failed: {}", fd, path,
                  strerror(e));
    return -e;
  }

  return 0;
}

int usvfs_open(const char* path, fuse_file_info* fi) noexcept
{
  logger::trace("usvfs_open(path='{}', flags={})", path, fi->flags);
  GET_STATE()
  FIND_ITEM()

  const int result = openat(state->fdMap.at(getParentPath(item->realPath())),
                            item->fileName().c_str(), fi->flags);
  if (result == -1) {
    return -errno;
  }

  fi->fh = result;

  return 0;
}

int usvfs_read(const char* path, char* buf, const size_t size, const off_t offset,
               fuse_file_info* fi) noexcept
{
  logger::trace("usvfs_read(path='{}', buf={}, size={}, offset={})", path,
                reinterpret_cast<long>(buf), size, offset);
  const int fd      = static_cast<int>(fi->fh);
  const ssize_t res = pread(fd, buf, size, offset);
  if (res == -1) {
    const int e = errno;
    logger::error("usvfs_read: pread failed: {}", strerror(e));
    return -e;
  }
  return static_cast<int>(res);
}

int usvfs_release(const char* path, fuse_file_info* fi) noexcept
{
  logger::trace("usvfs_release(path='{}')", path);
  if (fi && fi->fh != 0) {
    close(static_cast<int>(fi->fh));
    fi->fh = 0;
  }
  return 0;
}

int usvfs_write(const char* path, const char* buf, const size_t size,
                const off_t offset, fuse_file_info* fi) noexcept
{
  logger::trace("usvfs_write(path='{}', buf={}, size={}, offset={})", path,
                reinterpret_cast<long>(buf), size, offset);
  const ssize_t result = pwrite(static_cast<int>(fi->fh), buf, size, offset);
  if (result == -1) {
    const int e = errno;
    logger::error("usvfs_write: pwrite failed: {}", strerror(e));
    return -e;
  }
  return static_cast<int>(result);
}

int usvfs_statfs(const char* path, struct statvfs* stbuf) noexcept
{
  logger::trace("usvfs_statfs(path='{}')", path);

  GET_STATE()
  const int fd = state->fdMap.at(state->mountpoint);
  if (fstatvfs(fd, stbuf) < 0) {
    const int e = errno;
    logger::error("usvfs_statfs: fstatvfs({}:'{}') failed: {}", fd, state->mountpoint,
                  strerror(e));
    return -e;
  }

  return 0;
}

int usvfs_flush(const char* path, fuse_file_info* fi) noexcept
{
#warning STUB
  (void)fi;
  logger::warn("usvfs_flush(path='{}') - STUB!", path);
  return -ENOSYS;
}

int usvfs_fsync(const char* path, int isdatasync, fuse_file_info* fi) noexcept
{
#warning STUB
  (void)isdatasync;
  (void)fi;
  logger::warn("usvfs_fsync(path='{}') - STUB!", path);
  return -ENOSYS;
}

int usvfs_readdir(const char* path, void* buf, const fuse_fill_dir_t filler,
                  off_t /*offset*/, fuse_file_info* /*fi*/,
                  fuse_readdir_flags flags) noexcept
{
  logger::trace("usvfs_readdir(path='{}', flags={})", path, static_cast<int>(flags));

  GET_STATE()

  const auto tree = state->fileTree->find(path);
  if (tree == nullptr) {
    return -ENOENT;
  }

  const fuse_fill_dir_flags fill_flags = flags & FUSE_READDIR_PLUS
                                             ? FUSE_FILL_DIR_PLUS
                                             : static_cast<fuse_fill_dir_flags>(0);

  // Standard entries
  filler(buf, ".", nullptr, 0, fill_flags);
  filler(buf, "..", nullptr, 0, fill_flags);

  for (const auto& [itemName, item] : tree->getChildren()) {
    const string realPath       = item->realPath();
    const string realParentPath = getParentPath(realPath);

    struct stat stbuf;
    int fd = state->fdMap.at(realParentPath);
    if (fstatat(fd, item->fileName().c_str(), &stbuf, 0) == -1) {
      const int e = errno;
      logger::error("usvfs_readdir: fstatat({}:'{}', '{}'), itemName: '{}' failed: {}",
                    fd, realParentPath, item->fileName(), itemName, strerror(e));
      return -e;
    }

    if (filler(buf, itemName.c_str(), &stbuf, 0, fill_flags) != 0) {
      logger::error("usvfs_readdir: filler function returned error");
      break;
    }
  }

  return 0;
}

int usvfs_releasedir(const char* path, fuse_file_info* fi) noexcept
{
  logger::trace("usvfs_releasedir(path='{}')", path);
  if (fi && fi->fh != 0) {
    close(static_cast<int>(fi->fh));
    fi->fh = 0;
  }
  return 0;
}

int usvfs_fsyncdir(const char* path, int, fuse_file_info* fi) noexcept
{
#warning STUB
  (void)fi;
  logger::warn("usvfs_fsyncdir(path='{}') - STUB!", path);
  return -ENOSYS;
}

int usvfs_create(const char* path, mode_t mode, fuse_file_info* fi) noexcept
{
  logger::trace("usvfs_create(path='{}', mode={})", path, mode);
  GET_STATE()

  const string fileName   = getFileNameFromPath(path);
  const string parentPath = getParentPath(path);
  string realParentPath;
  if (state->upperDir.empty()) {
    auto parentItem = state->fileTree->find(parentPath);
    if (parentItem == nullptr) {
      logger::error(
          "usvfs_create: target parent directory '{}' does not exist in file tree",
          parentPath);
      return -ENOENT;
    }
    realParentPath = parentItem->realPath();
  } else {
    realParentPath = state->upperDir + parentPath;
  }
  int parentFd = state->fdMap.at(realParentPath);
  if (parentFd == -1 && !state->upperDir.empty()) {
    // parent path does not exist, this should only happen when upperDir is used
    parentFd = createParentDir(state, realParentPath, fileName, mode);
    if (parentFd < 0) {
      return parentFd;
    }
  }

  const int fd = openat(parentFd, fileName.c_str(), fi->flags, mode);
  if (fd < 0) {
    const int e = errno;
    logger::error("usvfs_create: openat({}:'{}', {}) failed: {}", parentFd,
                  realParentPath, fileName, strerror(e));
    return -e;
  }

  fi->fh = fd;

  auto item = state->fileTree->find(path);
  if (item == nullptr) {
    const auto newItem =
        state->fileTree->add(path, realParentPath + "/" + fileName, file);
    if (newItem == nullptr) {
      const int e = errno;
      logger::error("usvfs_create: error adding new file to file tree: {}",
                    strerror(e));
      return -e;
    }
  }

  return 0;
}
