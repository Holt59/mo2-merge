#ifndef MODINFODIALOGTAB_H
#define MODINFODIALOGTAB_H

#include "modinfo.h"
#include <QObject>

namespace MOShared { class FilesOrigin; }
namespace Ui { class ModInfoDialog; }

class Settings;
class OrganizerCore;

class ModInfoDialogTab : public QObject
{
  Q_OBJECT;

public:
  ModInfoDialogTab(const ModInfoDialogTab&) = delete;
  ModInfoDialogTab& operator=(const ModInfoDialogTab&) = delete;
  ModInfoDialogTab(ModInfoDialogTab&&) = default;
  ModInfoDialogTab& operator=(ModInfoDialogTab&&) = default;
  virtual ~ModInfoDialogTab() = default;

  virtual void clear() = 0;
  virtual void update();
  virtual bool feedFile(const QString& rootPath, const QString& filename);
  virtual bool canClose();
  virtual void saveState(Settings& s);
  virtual void restoreState(const Settings& s);
  virtual bool deleteRequested();

  virtual bool canHandleSeparators() const;
  virtual bool canHandleUnmanaged() const;

  virtual void setMod(ModInfo::Ptr mod, MOShared::FilesOrigin* origin);

  ModInfo::Ptr mod() const;
  MOShared::FilesOrigin* origin() const;

  int tabIndex() const;

signals:
  void originModified(int originID);
  void modOpen(QString name);

protected:
  Ui::ModInfoDialog* ui;

  ModInfoDialogTab(
    OrganizerCore& oc, PluginContainer& plugin,
    QWidget* parent, Ui::ModInfoDialog* ui, int index);

  OrganizerCore& core();
  PluginContainer& plugin();

  QWidget* parentWidget();

  void emitOriginModified();
  void emitModOpen(QString name);

private:
  OrganizerCore& m_core;
  PluginContainer& m_plugin;
  QWidget* m_parent;
  ModInfo::Ptr m_mod;
  MOShared::FilesOrigin* m_origin;
  int m_tabIndex;
};


class NotesTab : public ModInfoDialogTab
{
public:
  NotesTab(
    OrganizerCore& oc, PluginContainer& plugin,
    QWidget* parent, Ui::ModInfoDialog* ui, int index);

  void clear() override;
  void update() override;
  bool canHandleSeparators() const override;

private:
  void onComments();
  void onNotes();
};

#endif // MODINFODIALOGTAB_H
