#include "virtualfiletreeitem.h"

#include "logger.h"
#include "utils.h"

using namespace std;
namespace fs = std::filesystem;

VirtualFileTreeItem::VirtualFileTreeItem(std::string path, std::string realPath,
                                         Type type,
                                         VirtualFileTreeItem* parent) noexcept(false)
    : m_fileName(std::move(path)), m_realPath(std::move(realPath)), m_parent(parent),
      m_type(type), m_deleted(false)
{
  logger::trace("{}: '{}', '{}'", __FUNCTION__, m_fileName, m_realPath);
  if (m_fileName.empty()) {
    throw runtime_error("filename is empty");
  }

  if (m_realPath.empty()) {
    throw runtime_error(format("real path is empty"));
  }
}

VirtualFileTreeItem::VirtualFileTreeItem(std::string path, std::string realPath,
                                         VirtualFileTreeItem* parent) noexcept(false)
    : m_fileName(std::move(path)), m_realPath(std::move(realPath)), m_parent(parent),
      m_deleted(false)
{
  if (fs::exists(realPath)) {
    if (fs::status(realPath).type() == fs::file_type::directory) {
      m_type = dir;
    } else {
      m_type = file;
    }
  } else {
    m_type = unknown;
  }
}

VirtualFileTreeItem::VirtualFileTreeItem(const VirtualFileTreeItem& other) noexcept
    : m_fileName(other.m_fileName), m_realPath(other.m_realPath),
      m_parent(other.m_parent), m_type(other.m_type), m_deleted(other.m_deleted)
{
  shared_lock lock(other.m_mtx);
  for (const auto& [name, item] : other.m_children) {
    auto childClone      = make_shared<VirtualFileTreeItem>(*item);
    childClone->m_parent = this;
    m_children.emplace(name, std::move(childClone));
  }
}

VirtualFileTreeItem&
VirtualFileTreeItem::operator+=(const VirtualFileTreeItem& other) noexcept
{
  unique_lock lock(m_mtx);
  shared_lock lock_other(other.m_mtx);

  m_realPath = other.m_realPath;

  for (const auto& [name, item] : other.m_children) {
    // try to insert a nullptr
    auto [it, wasInserted] = m_children.try_emplace(name, nullptr);
    if (wasInserted) {
      // item did not exist, replace nullptr with a clone
      it->second = item->clone();
    } else {
      // item already exists, merge recursively
      *it->second += *item;
    }
  }

  return *this;
}

std::shared_ptr<VirtualFileTreeItem>
VirtualFileTreeItem::add(std::string_view path, std::string realPath, Type type,
                         bool updateExisting) noexcept
{
  unique_lock lock(m_mtx);

  if (path.empty() || realPath.empty()) {
    logger::error("attempted to add an entry with empty path!");
    errno = EINVAL;
    return nullptr;
  }

  if (path[0] == '/') {
    path.remove_prefix(1);
  }

  return addInternal(path, toLower(path), std::move(realPath), type, updateExisting);
}

std::shared_ptr<VirtualFileTreeItem>
VirtualFileTreeItem::add(std::string_view path, std::string realPath,
                         bool updateExisting) noexcept
{
  Type type;
  if (fs::exists(realPath)) {
    if (fs::status(realPath).type() == fs::file_type::directory) {
      type = dir;
    } else {
      type = file;
    }
  } else {
    type = unknown;
  }

  return add(path, std::move(realPath), type, updateExisting);
}

std::shared_ptr<VirtualFileTreeItem> VirtualFileTreeItem::clone() const noexcept
{
  try {
    return make_shared<VirtualFileTreeItem>(*this);
  } catch (const std::bad_alloc&) {
    errno = ENOMEM;
    return nullptr;
  }
}

VirtualFileTreeItem* VirtualFileTreeItem::getParent() const noexcept
{
  shared_lock lock(m_mtx);
  return m_parent;
}

Type VirtualFileTreeItem::getType() const noexcept
{
  shared_lock lock(m_mtx);
  return m_type;
}

void VirtualFileTreeItem::setType(Type type) noexcept
{
  unique_lock lock(m_mtx);
  m_type = type;
}

bool VirtualFileTreeItem::erase(std::string_view path, bool reallyErase) noexcept
{
  unique_lock lock(m_mtx);

  if (path.empty()) {
    logger::error("attempted to call {} with an empty path", __FUNCTION__);
    errno = EINVAL;
    return false;
  }

  // remove leading '/'
  if (path[0] == '/') {
    path.remove_prefix(1);
  }

  return eraseInternal(toLower(path), reallyErase);
}

VirtualFileTreeItem* VirtualFileTreeItem::find(std::string_view path,
                                               bool includeDeleted) noexcept
{
  shared_lock lock(m_mtx);

  if (path == "/" || path.empty()) {
    return this;
  }

  if (path[0] == '/') {
    path.remove_prefix(1);
  }

  return findInternal(toLower(path), includeDeleted);
}

std::string VirtualFileTreeItem::fileName() const noexcept
{
  shared_lock lock(m_mtx);
  return m_fileName;
}

std::string VirtualFileTreeItem::filePath() const noexcept
{
  shared_lock lock(m_mtx);
  if (m_parent == nullptr) {
    return "";
  }

  string parentFilePath = m_parent->filePath();
  if (!parentFilePath.ends_with('/')) {
    parentFilePath += "/";
  }

  return parentFilePath + m_fileName;
}

std::string VirtualFileTreeItem::realPath() const noexcept
{
  shared_lock lock(m_mtx);
  return m_realPath;
}

void VirtualFileTreeItem::setName(std::string name) noexcept
{
  if (name.empty()) {
    logger::error("attempted to call {} with an empty parameter", __FUNCTION__);
    errno = EINVAL;
    return;
  }
  unique_lock lock(m_mtx);
  m_fileName = std::move(name);
}

void VirtualFileTreeItem::setRealPath(std::string realPath) noexcept
{
  if (realPath.empty()) {
    logger::error("attempted to call {} with an empty parameter", __FUNCTION__);
    errno = EINVAL;
    return;
  }

  unique_lock lock(m_mtx);
  m_realPath = std::move(realPath);
}

bool VirtualFileTreeItem::isDeleted() const noexcept
{
  shared_lock lock(m_mtx);
  return m_deleted;
}

void VirtualFileTreeItem::setDeleted(bool deleted) noexcept
{
  unique_lock lock(m_mtx);
  m_deleted = deleted;
}

bool VirtualFileTreeItem::isEmpty() const noexcept
{
  shared_lock lock(m_mtx);
  return isEmptyInternal();
}

const FileMap& VirtualFileTreeItem::getChildren() const noexcept
{
  shared_lock lock(m_mtx);
  return m_children;
}

bool VirtualFileTreeItem::isDir() const noexcept
{
  shared_lock lock(m_mtx);
  return m_type == dir;
}

bool VirtualFileTreeItem::isFile() const noexcept
{
  shared_lock lock(m_mtx);
  return m_type == file;
}

std::vector<const VirtualFileTreeItem*>
VirtualFileTreeItem::getAllItems(bool includeRoot) const noexcept
{
  shared_lock lock(m_mtx);
  vector<const VirtualFileTreeItem*> result;
  result.reserve(m_children.size() + 1);

  if (m_parent != nullptr || includeRoot) {
    result.emplace_back(this);
  }
  for (const auto& item : m_children | views::values) {
    auto allItems = item->getAllItems();
    result.insert(result.end(), allItems.begin(), allItems.end());
  }
  return result;
}

std::vector<std::string>
VirtualFileTreeItem::getAllItemPaths(bool includeRoot) const noexcept
{
  shared_lock lock(m_mtx);
  vector<string> result;
  result.reserve(m_children.size() + 1);

  if (m_parent != nullptr || includeRoot) {
    result.emplace_back(filePath());
  }
  for (const auto& item : m_children | views::values) {
    auto allItemPaths = item->getAllItemPaths();
    result.insert(result.end(), allItemPaths.begin(), allItemPaths.end());
  }
  return result;
}

void VirtualFileTreeItem::dumpTree(std::ostream& os, int level) const noexcept
{
  shared_lock lock(m_mtx);

  // append '/' to directories
  string filename = fileName();
  if (isDir()) {
    if (!filename.ends_with('/')) {
      filename.append("/");
    }
  }

  os << string(level, ' ') << filename << " -> " << realPath() << '\n';
  for (const auto& child : m_children | views::values) {
    child->dumpTree(os, level + 1);
  }
}

VirtualFileTreeItem* VirtualFileTreeItem::findInternal(std::string_view path,
                                                       bool includeDeleted) noexcept
{
  const size_t pos = path.find('/');
  if (pos != string::npos) {
    // path is inside a subdirectory
    const string_view subDirectory = path.substr(0, pos);
    const auto it                  = m_children.find(subDirectory);
    if (it == m_children.end()) {
      logger::debug("could not find '{}'", path);
      errno = ENOENT;
      return nullptr;
    }

    const size_t nextPos = path.find('/', subDirectory.length());
    if (nextPos == string_view::npos) {
      // no further subdirectories
      if (!it->second->isDeleted() || includeDeleted) {
        return it->second.get();
      }
      logger::debug("'{}' has been deleted, returning nullptr", path);
      errno = ENOENT;
      return nullptr;
    }
    return it->second->findInternal(path.substr(nextPos + 1), includeDeleted);
  }

  // path is not in a subdirectory
  const auto it = m_children.find(path);
  if (it == m_children.end()) {
    logger::debug("could not find '{}'", path);
    errno = ENOENT;
    return nullptr;
  }

  if (!it->second->isDeleted() || includeDeleted) {
    return it->second.get();
  }
  logger::debug("'{}' has been deleted, returning nullptr", path);
  errno = ENOENT;
  return nullptr;
}

std::shared_ptr<VirtualFileTreeItem>
VirtualFileTreeItem::addInternal(std::string_view path, std::string_view pathLc,
                                 std::string realPath, Type type,
                                 bool updateExisting) noexcept
{
  if (path == "/") {
    errno = EEXIST;
    return nullptr;
  }

  // remove leading '/'
  if (path[0] == '/') {
    path.remove_prefix(1);
    pathLc.remove_prefix(1);
  }

  if (const size_t pos = path.find('/', 0); pos != string::npos) {
    string subDirectory = string(pathLc.substr(0, pos));

    auto foundEntry = m_children.find(subDirectory);

    if (foundEntry == m_children.end()) {
      logger::error("subdirectory does not exist");
      errno = ENOENT;
      return nullptr;
    }
    return foundEntry->second->addInternal(path.substr(pos + 1), pathLc.substr(pos + 1),
                                           std::move(realPath), type, updateExisting);
  }

  auto [it, wasInserted] = m_children.try_emplace(string(pathLc), nullptr);
  if (!wasInserted) {
    if (!updateExisting) {
      logger::error("item '{}' already exists and should not be updated", path);
      errno = EEXIST;
      return nullptr;
    }
    logger::debug("setting real path of existing item '{}' to '{}'", path, realPath);
    it->second->m_realPath = realPath;

    return it->second;
  }

  it->second = make_shared<VirtualFileTreeItem>(string(path), realPath, type, this);
  return it->second;
}

bool VirtualFileTreeItem::isEmptyInternal() const noexcept
{
  return ranges::all_of(m_children, [](const auto& entry) {
    return entry.second->isEmpty();
  });
}

bool VirtualFileTreeItem::eraseInternal(std::string_view path,
                                        bool reallyErase) noexcept
{
  // check if path is inside a subdirectory
  if (const size_t pos = path.find('/', 0); pos != string::npos) {
    string_view subDir = path.substr(0, pos);
    const auto it      = m_children.find(subDir);

    if (it == m_children.end()) {
      errno = ENOENT;
      logger::debug("subdirectory {} not found", subDir);
      return false;
    }

    return it->second->eraseInternal(path.substr(pos + 1));
  }

  const auto it = m_children.find(path);

  // check if the entry exists
  if (it == m_children.end()) {
    errno = ENOENT;
    logger::debug("{} not found", path);
    return false;
  }

  // check if the entry is empty
  if (!it->second->isEmpty()) {
    errno = ENOTEMPTY;
    return false;
  }

  if (reallyErase) {
    m_children.erase(it);
    return true;
  }
  it->second->setDeleted(true);
  return true;
}

std::ostream& operator<<(std::ostream& os, const VirtualFileTreeItem& item) noexcept
{
  shared_lock lock(item.m_mtx);
  os << "file path: " << quoted(item.filePath())
     << ", real path: " << quoted(item.m_realPath) << '\n';
  for (const auto& child : item.m_children | views::values) {
    os << *child;
  }

  return os;
}
