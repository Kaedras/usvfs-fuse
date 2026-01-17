#pragma once

#include <map>
#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

class VirtualFileTreeItem;
using FileMap =
    std::map<std::string, std::shared_ptr<VirtualFileTreeItem>, std::less<>>;

enum Type
{
  file,
  dir,
  unknown
};

class VirtualFileTreeItem
{
public:
  VirtualFileTreeItem() = delete;
  VirtualFileTreeItem(std::string path, std::string realPath, Type type,
                      VirtualFileTreeItem* parent = nullptr) noexcept(false);
  VirtualFileTreeItem(std::string path, std::string realPath,
                      VirtualFileTreeItem* parent = nullptr) noexcept(false);

  VirtualFileTreeItem(const VirtualFileTreeItem& other) noexcept;

  ~VirtualFileTreeItem() = default;

  VirtualFileTreeItem& operator+=(const VirtualFileTreeItem& other) noexcept;

  /**
   * @brief Add a new item to the virtual file tree
   *
   * This method either creates a new item or updates the path of an existing item,
   * depending on the provided parameters and the current state of the tree
   *
   * @param path The path of the item to add or update
   * @param realPath The full real path corresponding to the item in the filesystem
   * @param type The type of the new item, e.g. std::filesystem::directory
   * @param updateExisting Indicates whether to update the real path of an existing item
   * if it already exists. If false, the method will not overwrite existing items
   * @return True if the item was successfully added or updated, false otherwise
   */
  std::shared_ptr<VirtualFileTreeItem> add(std::string_view path, std::string realPath,
                                           Type type,
                                           bool updateExisting = false) noexcept;
  std::shared_ptr<VirtualFileTreeItem> add(std::string_view path, std::string realPath,
                                           bool updateExisting = false) noexcept;

  std::shared_ptr<VirtualFileTreeItem> clone() const noexcept;

  VirtualFileTreeItem* getParent() const noexcept;

  Type getType() const noexcept;
  void setType(Type type) noexcept;

  /**
   * @brief Erase a file or directory from the file tree. Directories must be empty
   * @param path Path to remove
   * @param reallyErase Whether to delete the item from the tree or to mark it as
   * deleted
   * @return True if the item was successfully removed, false otherwise. See errno for
   * error details
   */
  bool erase(std::string_view path, bool reallyErase = true) noexcept;

  [[nodiscard]] VirtualFileTreeItem* find(std::string_view path,
                                          bool includeDeleted = false) noexcept;

  /**
   * @brief Get the file name
   * @note Returns empty string for root items
   */
  [[nodiscard]] std::string fileName() const noexcept;

  /**
   * @brief Get the file path including all ancestors
   * @note Returns empty string for root items
   */
  [[nodiscard]] std::string filePath() const noexcept;

  /**
   * @brief Get the real path
   */
  [[nodiscard]] std::string realPath() const noexcept;

  /**
   * @brief Set the file name
   */
  void setName(std::string name) noexcept;

  /**
   * @brief Set the real path
   */
  void setRealPath(std::string realPath) noexcept;

  bool isDeleted() const noexcept;

  void setDeleted(bool deleted) noexcept;

  /**
   * @brief Check whether there are any children that are not marked as deleted
   */
  bool isEmpty() const noexcept;

  [[nodiscard]] const FileMap& getChildren() const noexcept;

  bool isDir() const noexcept;
  bool isFile() const noexcept;

  [[nodiscard]] std::vector<const VirtualFileTreeItem*>
  getAllItems(bool includeRoot = true) const noexcept;
  [[nodiscard]] std::vector<std::string>
  getAllItemPaths(bool includeRoot = true) const noexcept;

  friend std::ostream& operator<<(std::ostream& os,
                                  const VirtualFileTreeItem& item) noexcept;

  void dumpTree(std::ostream& os, int level = 0) const noexcept;

private:
  std::string m_fileName;
  std::string m_realPath;
  VirtualFileTreeItem* m_parent;
  Type m_type;
  bool m_deleted;
  FileMap m_children;
  mutable std::shared_mutex m_mtx;

  // find function without locking
  [[nodiscard]] VirtualFileTreeItem* findInternal(std::string_view path,
                                                  bool includeDeleted) noexcept;

  // add function without locking for internal use
  std::shared_ptr<VirtualFileTreeItem>
  addInternal(std::string_view path, std::string_view pathLc, std::string realPath,
              Type type, bool updateExisting = false) noexcept;

  // isEmpty function without locking
  bool isEmptyInternal() const noexcept;
  bool eraseInternal(std::string_view path, bool reallyErase = true) noexcept;
};
