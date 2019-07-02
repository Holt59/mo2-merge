#include "modinfodialogesps.h"
#include "ui_modinfodialog.h"
#include "modinfodialog.h"
#include <report.h>

using MOBase::reportError;

class ESPItem
{
public:
  ESPItem(QString rootPath, QString relativePath)
    : m_rootPath(std::move(rootPath)), m_active(false)
  {
    if (relativePath.contains('/')) {
      m_inactivePath = relativePath;
    } else {
      m_activePath = relativePath;
      m_active = true;
    }

    updateFilename();
  }

  const QString& rootPath() const
  {
    return m_rootPath;
  }

  const QString& relativePath() const
  {
    if (m_active) {
      return m_activePath;
    } else {
      return m_inactivePath;
    }
  }

  const QString& filename() const
  {
    return m_filename;
  }

  const QString& activePath() const
  {
    return m_activePath;
  }

  const QString& inactivePath() const
  {
    return m_inactivePath;
  }

  QFileInfo fileInfo() const
  {
    return m_rootPath + QDir::separator() + relativePath();
  }

  bool isActive() const
  {
    return m_active;
  }

  bool activate(const QString& newName)
  {
    QDir root(m_rootPath);

    if (root.rename(m_inactivePath, newName)) {
      m_active = true;
      m_activePath = newName;

      if (QFileInfo(m_inactivePath).fileName() != newName) {
        // file was renamed
        m_inactivePath = QFileInfo(m_inactivePath).path() + QDir::separator() + newName;
      }

      updateFilename();

      return true;
    }

    return false;
  }

  bool deactivate(const QString& newName)
  {
    QDir root(m_rootPath);

    if (root.rename(m_activePath, newName)) {
      m_active = false;
      m_inactivePath = newName;
      updateFilename();
      return true;
    }

    return false;
  }

private:
  QString m_rootPath;
  QString m_activePath;
  QString m_inactivePath;
  QString m_filename;
  bool m_active;

  void updateFilename()
  {
    m_filename = fileInfo().fileName();
  }
};


class ESPListModel : public QAbstractItemModel
{
public:
  void clear()
  {
    m_esps.clear();
    endResetModel();
  }

  QModelIndex index(int row, int col, const QModelIndex& ={}) const override
  {
    return createIndex(row, col);
  }

  QModelIndex parent(const QModelIndex&) const override
  {
    return {};
  }

  int rowCount(const QModelIndex& ={}) const override
  {
    return static_cast<int>(m_esps.size());
  }

  int columnCount(const QModelIndex& ={}) const override
  {
    return 1;
  }

  QVariant data(const QModelIndex& index, int role) const override
  {
    if (role == Qt::DisplayRole) {
      if (auto* esp=getESP(index)) {
        return esp->filename();
      }
    }

    return {};
  }


  void add(ESPItem esp)
  {
    m_esps.emplace_back(std::move(esp));
  }

  void addOne(ESPItem esp)
  {
    const auto i = m_esps.size();

    beginInsertRows({}, static_cast<int>(i), static_cast<int>(i));
    add(std::move(esp));
    endInsertRows();
  }

  bool removeRows(int row, int count, const QModelIndex& = {}) override
  {
    if (row < 0) {
      return false;
    }

    const auto start = static_cast<std::size_t>(row);
    if (start >= m_esps.size()) {
      return false;
    }

    const auto end = std::min(
      start + static_cast<std::size_t>(count),
      m_esps.size());

    beginRemoveRows({}, static_cast<int>(start), static_cast<int>(end));
    m_esps.erase(m_esps.begin() + start, m_esps.begin() + end);
    endRemoveRows();

    return true;
  }

  void finished()
  {
    std::sort(m_esps.begin(), m_esps.end(), [](const auto& a, const auto& b) {
      return (naturalCompare(a.filename(), b.filename()) < 0);
    });

    endResetModel();
  }

  const ESPItem* getESP(const QModelIndex& index) const
  {
    const auto row = index.row();
    if (row < 0) {
      return nullptr;
    }

    const auto i = static_cast<std::size_t>(row);
    if (i >= m_esps.size()) {
      return nullptr;
    }

    return &m_esps[i];
  }

  ESPItem* getESP(const QModelIndex& index)
  {
    return const_cast<ESPItem*>(std::as_const(*this).getESP(index));
  }

private:
  std::deque<ESPItem> m_esps;
};



ESPsTab::ESPsTab(
  OrganizerCore& oc, PluginContainer& plugin,
  QWidget* parent, Ui::ModInfoDialog* ui, int id) :
    ModInfoDialogTab(oc, plugin, parent, ui, id),
    m_inactiveModel(new ESPListModel), m_activeModel(new ESPListModel)
{
  ui->inactiveESPList->setModel(m_inactiveModel);
  ui->activeESPList->setModel(m_activeModel);

  QObject::connect(
    ui->activateESP, &QToolButton::clicked, [&]{ onActivate(); });

  QObject::connect(
    ui->deactivateESP, &QToolButton::clicked, [&]{ onDeactivate(); });
}

void ESPsTab::clear()
{
  m_inactiveModel->clear();
  m_activeModel->clear();
  setHasData(false);
}

bool ESPsTab::feedFile(const QString& rootPath, const QString& fullPath)
{
  static constexpr const char* extensions[] = {
    ".esp", ".esm", ".esl"
  };

  for (const auto* e : extensions) {
    if (fullPath.endsWith(e, Qt::CaseInsensitive)) {
      QString relativePath = fullPath.mid(rootPath.length() + 1);

      ESPItem esp(rootPath, relativePath);

      if (esp.isActive()) {
        m_activeModel->add(std::move(esp));
      } else {
        m_inactiveModel->add(std::move(esp));
      }

      setHasData(true);
      return true;
    }
  }

  return false;
}

void ESPsTab::update()
{
  m_inactiveModel->finished();
  m_activeModel->finished();
}

void ESPsTab::onActivate()
{
  const auto index = ui->inactiveESPList->currentIndex();
  if (!index.isValid()) {
    return;
  }

  auto* esp = m_inactiveModel->getESP(index);
  if (!esp) {
    return;
  }

  if (esp->isActive()) {
    qWarning("ESPsTab::onActive(): item is already active");
    return;
  }

  QDir root(esp->rootPath());
  const QFileInfo file(esp->fileInfo());

  QString newName = file.fileName();

  while (root.exists(newName)) {
    bool okClicked = false;

    newName = QInputDialog::getText(
      parentWidget(),
      QObject::tr("File Exists"),
      QObject::tr("A file with that name exists, please enter a new one"),
      QLineEdit::Normal, file.fileName(), &okClicked);

    if (!okClicked) {
      return;
    }

    if (newName.isEmpty()) {
      newName = file.fileName();
    }
  }

  if (esp->activate(newName)) {
    // copy esp, original will be destroyed
    auto copy = *esp;
    m_inactiveModel->removeRow(index.row());
    m_activeModel->addOne(std::move(copy));
    selectRow(ui->inactiveESPList, index.row());
  } else {
    reportError(QObject::tr("Failed to move file"));
  }
}

void ESPsTab::onDeactivate()
{
  const auto index = ui->activeESPList->currentIndex();
  if (!index.isValid()) {
    return;
  }

  auto* esp = m_activeModel->getESP(index);
  if (!esp) {
    return;
  }

  if (!esp->isActive()) {
    qWarning("ESPsTab::onDeactivate(): item is already inactive");
    return;
  }

  QDir root(esp->rootPath());

  // if we moved the file from optional to active in this session, we move the
  // file back to where it came from. Otherwise, it is moved to the new folder
  // "optional"

  QString newName = esp->inactivePath();

  if (newName.isEmpty()) {
    if (!root.exists("optional")) {
      if (!root.mkdir("optional")) {
        reportError(QObject::tr("Failed to create directory \"optional\""));
        return;
      }
    }

    newName = QString("optional") + QDir::separator() + esp->fileInfo().fileName();
  }

  if (esp->deactivate(newName)) {
    // copy esp, original will be destroyed
    auto copy = *esp;

    m_activeModel->removeRow(index.row());
    m_inactiveModel->addOne(std::move(copy));
    selectRow(ui->activeESPList, index.row());
  } else {
    reportError(QObject::tr("Failed to move file"));
  }
}

void ESPsTab::selectRow(QListView* list, int row)
{
  const auto* model = list->model();
  const auto count = model->rowCount();
  if (count == 0) {
    return;
  }

  if (row >= count) {
    list->setCurrentIndex(model->index(count - 1, 0));
  } else {
    list->setCurrentIndex(model->index(row, 0));
  }
}
