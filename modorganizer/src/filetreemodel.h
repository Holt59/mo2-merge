#ifndef MODORGANIZER_FILETREEMODEL_INCLUDED
#define MODORGANIZER_FILETREEMODEL_INCLUDED

#include "filetreeitem.h"
#include "iconfetcher.h"
#include "directoryentry.h"

class OrganizerCore;

class FileTreeModel : public QAbstractItemModel
{
  Q_OBJECT;

public:
  enum Flag
  {
    NoFlags   = 0x00,
    Conflicts = 0x01,
    Archives  = 0x02
  };

  Q_DECLARE_FLAGS(Flags, Flag);

  FileTreeModel(OrganizerCore& core, QObject* parent=nullptr);

  void setFlags(Flags f);
  void refresh();

  QModelIndex index(int row, int col, const QModelIndex& parent={}) const override;
  QModelIndex parent(const QModelIndex& index) const override;
  int rowCount(const QModelIndex& parent={}) const override;
  int columnCount(const QModelIndex& parent={}) const override;
  bool hasChildren(const QModelIndex& parent={}) const override;
  QVariant data(const QModelIndex& index, int role=Qt::DisplayRole) const override;
  QVariant headerData(int i, Qt::Orientation ori, int role=Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  FileTreeItem* itemFromIndex(const QModelIndex& index) const;

private:
  enum class FillFlag
  {
    None             = 0x00,
    PruneDirectories = 0x01
  };

  Q_DECLARE_FLAGS(FillFlags, FillFlag);

  using DirectoryIterator = std::vector<MOShared::DirectoryEntry*>::const_iterator;
  OrganizerCore& m_core;
  mutable FileTreeItem m_root;
  Flags m_flags;
  mutable IconFetcher m_iconFetcher;
  mutable std::vector<QModelIndex> m_iconPending;
  mutable QTimer m_iconPendingTimer;

  bool showConflicts() const;
  bool showArchives() const;

  void fill(
    FileTreeItem& parentItem, const MOShared::DirectoryEntry& parentEntry,
    const std::wstring& parentPath);

  void fillDirectories(
    FileTreeItem& parentItem, const std::wstring& path,
    DirectoryIterator begin, DirectoryIterator end, FillFlags flags);

  void fillFiles(
    FileTreeItem& parentItem, const std::wstring& path,
    const std::vector<MOShared::FileEntry::Ptr>& files, FillFlags flags);


  void update(
    FileTreeItem& parentItem, const MOShared::DirectoryEntry& parentEntry,
    const std::wstring& parentPath);

  void updateDirectories(
    FileTreeItem& parentItem, const std::wstring& path,
    const MOShared::DirectoryEntry& parentEntry, FillFlags flags);

  void updateFiles(
    FileTreeItem& parentItem, const std::wstring& path,
    const MOShared::DirectoryEntry& parentEntry, FillFlags flags);

  std::wstring makeModName(const MOShared::FileEntry& file, int originID) const;

  void ensureLoaded(FileTreeItem* item) const;
  void updatePendingIcons();
  void removePendingIcons(const QModelIndex& parent, int first, int last);

  bool shouldShowFile(const MOShared::FileEntry& file) const;
  bool hasFilesAnywhere(const MOShared::DirectoryEntry& dir) const;
  QString makeTooltip(const FileTreeItem& item) const;
  QVariant makeIcon(const FileTreeItem& item, const QModelIndex& index) const;

  QModelIndex indexFromItem(FileTreeItem* item, int row, int col) const;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FileTreeModel::Flags);
Q_DECLARE_OPERATORS_FOR_FLAGS(FileTreeItem::Flags);

#endif // MODORGANIZER_FILETREEMODEL_INCLUDED
