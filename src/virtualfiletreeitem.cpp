#include "usvfs-fuse/virtualfiletreeitem.h"

#include "usvfs-fuse/logger.h"
#include "usvfs-fuse/utils.h"

using namespace std;
namespace fs = std::filesystem;

VirtualFileTreeItem::VirtualFileTreeItem(std::string name, std::string realPath,
                                         Type type,
                                         VirtualFileTreeItem* parent) noexcept(false)
    : m_fileName(std::move(name)), m_realPath(std::move(realPath)), m_parent(parent),
      m_type(type), m_deleted(false)
{
  logger::trace("{}: '{}', '{}'", __FUNCTION__, m_fileName, m_realPath);
  if (m_fileName.empty()) {
    throw runtime_error("item name is empty");
  }

  if (m_realPath.empty()) {
    throw runtime_error(format("real path is empty"));
  }
}

VirtualFileTreeItem::VirtualFileTreeItem(std::string name, std::string realPath,
                                         VirtualFileTreeItem* parent) noexcept(false)
    : m_fileName(std::move(name)), m_realPath(std::move(realPath)), m_parent(parent),
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

VirtualFileTreeItem::VirtualFileTreeItem(const VirtualFileTreeItem& other)
    : m_fileName(other.m_fileName), m_realPath(other.m_realPath),
      m_parent(other.m_parent), m_type(other.m_type), m_deleted(other.m_deleted)
{
  for (const auto& otherItem : other.getAllItems(false)) {
    addInternal(otherItem->filePath(), otherItem->realPath(), otherItem->m_type);
  }
}

VirtualFileTreeItem& VirtualFileTreeItem::operator+=(const VirtualFileTreeItem& other)
{
  unique_lock lock(m_mtx);
  m_realPath = other.m_realPath;

  for (const auto& otherItem : other.getAllItems(false)) {
    addInternal(otherItem->filePath(), otherItem->realPath(), otherItem->m_type, true);
  }

  return *this;
}

std::shared_ptr<VirtualFileTreeItem>
VirtualFileTreeItem::add(std::string name, std::string realPath, Type type,
                         bool updateExisting) noexcept
{
  if (name.empty() || realPath.empty()) {
    logger::error("attempted to add an entry with empty name!");
    errno = EINVAL;
    return nullptr;
  }
  unique_lock lock(m_mtx);
  return addInternal(std::move(name), std::move(realPath), type, updateExisting);
}

std::shared_ptr<VirtualFileTreeItem>
VirtualFileTreeItem::add(std::string name, std::string realPath,
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

  return add(std::move(name), std::move(realPath), type, updateExisting);
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

bool VirtualFileTreeItem::erase(std::string name, bool reallyErase) noexcept
{
  if (name.empty()) {
    logger::error("attempted to call {} with an empty name", __FUNCTION__);
    errno = EINVAL;
    return false;
  }

  unique_lock lock(m_mtx);
  // remove leading '/'
  if (name[0] == '/') {
    name.erase(0, 1);
  }

  // convert name to lower case
  toLowerInplace(name);

  if (const size_t pos = name.find('/', 0); pos != string::npos) {
    std::string subDirectory = name.substr(0, pos);
    const auto foundEntry    = m_children.find(subDirectory);

    if (foundEntry == m_children.end()) {
      errno = ENOENT;
      logger::debug("subdirectory {} not found", subDirectory);
      return false;
    }

    return foundEntry->second->erase(name.substr(pos + 1));
  }

  const auto foundEntry = m_children.find(name);

  // check if the entry exists
  if (foundEntry == m_children.end()) {
    errno = ENOENT;
    logger::debug("{} not found", name);
    return false;
  }

  // check if the entry is empty
  if (!foundEntry->second->getChildren().empty()) {
    errno = ENOTEMPTY;
    return false;
  }

  if (reallyErase) {
    return m_children.erase(name);
  }
  foundEntry->second->setDeleted(true);
  return true;
}

VirtualFileTreeItem* VirtualFileTreeItem::find(std::string_view value,
                                               bool includeDeleted) noexcept
{
  shared_lock lock(m_mtx);
  // convert name to lower case
  const string lower = toLower(value);

  return findPrivate(lower, includeDeleted);
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
  std::vector<const VirtualFileTreeItem*> result;
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
  std::vector<string> result;
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

void VirtualFileTreeItem::dumpTree(std::ostream& os, int level) const
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

VirtualFileTreeItem* VirtualFileTreeItem::findPrivate(std::string_view value,
                                                      bool includeDeleted) noexcept
{
  shared_lock lock(m_mtx);
  // special case: return `this` if it is the root item
  if (value == "/" || value.empty()) {
    // sanity check
    if (m_parent != nullptr) {
      logger::warn(
          "findPrivate() was called with parameter '{}', but m_parent is not nullptr",
          value);
      errno = EINVAL;
      return nullptr;
    }

    logger::debug("findPrivate() was called with parameter '{}', returning 'this'",
                  value);
    return this;
  }

  // remove leading '/'
  if (value[0] == '/') {
    value.remove_prefix(1);
  }

  if (const size_t pos = value.find('/'); pos != string::npos) {
    const string_view subDirectory = value.substr(0, pos);

    // the item is in a subdirectory
    const auto it = m_children.find(string(subDirectory));
    if (it == m_children.end()) {
      logger::debug("could not find '{}'", value);
      errno = ENOENT;
      return nullptr;
    }

    const size_t nextPos = value.find('/', subDirectory.length());
    if (nextPos == string_view::npos) {
      // no further subdirectories
      if (!it->second->isDeleted() || includeDeleted) {
        return it->second.get();
      }
      logger::debug("'{}' has been deleted, returning nullptr", value);
      errno = ENOENT;
      return nullptr;
    }
    return it->second->findPrivate(value.substr(nextPos), includeDeleted);
  }

  // item is not in a subdirectory
  const auto it = m_children.find(string(value));
  if (it == m_children.end()) {
    logger::debug("could not find '{}'", value);
    errno = ENOENT;
    return nullptr;
  }

  if (!it->second->isDeleted() || includeDeleted) {
    return it->second.get();
  }
  logger::debug("'{}' has been deleted, returning nullptr", value);
  errno = ENOENT;
  return nullptr;
}

std::shared_ptr<VirtualFileTreeItem>
VirtualFileTreeItem::addInternal(std::string name, std::string realPath, Type type,
                                 bool updateExisting) noexcept
{
  if (name == "/") {
    errno = EEXIST;
    return nullptr;
  }

  // remove leading '/'
  if (name[0] == '/') {
    name.erase(0, 1);
  }

  // convert name to lower case
  const string nameLc = toLower(name);

  if (const size_t pos = name.find('/', 0); pos != std::string::npos) {
    std::string subDirectory = nameLc.substr(0, pos);

    auto foundEntry = m_children.find(subDirectory);

    if (foundEntry == m_children.end()) {
      logger::error("subdirectory does not exist");
      errno = ENOENT;
      return nullptr;
    }
    return foundEntry->second->add(name.substr(pos + 1), std::move(realPath), type,
                                   updateExisting);
  }

  // check if the item already exists
  if (const auto existingItem = m_children.find(name);
      existingItem != m_children.end()) {
    if (!updateExisting) {
      logger::error("item '{}' already exists and should not be updated", name);
      errno = EEXIST;
      return nullptr;
    }
    logger::debug("setting real path of existing item '{}' to '{}'", name, realPath);
    existingItem->second->m_realPath = realPath;

    return existingItem->second;
  }

  auto [newItem, wasInserted] = m_children.emplace(
      nameLc, make_shared<VirtualFileTreeItem>(name, realPath, type, this));
  if (!wasInserted) {
    logger::error("m_children.emplace(key='{}') failed on file '{}'", nameLc, name);
    return nullptr;
  }
  return newItem->second;
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
