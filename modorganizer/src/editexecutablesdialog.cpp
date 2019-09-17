/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "editexecutablesdialog.h"
#include "ui_editexecutablesdialog.h"
#include "filedialogmemory.h"
#include "stackdata.h"
#include "modlist.h"
#include "forcedloaddialog.h"
#include "organizercore.h"

#include <QMessageBox>
#include <Shellapi.h>
#include <utility.h>
#include <algorithm>

using namespace MOBase;
using namespace MOShared;

EditExecutablesDialog::EditExecutablesDialog(OrganizerCore& oc, QWidget* parent)
  : TutorableDialog("EditExecutables", parent)
  , ui(new Ui::EditExecutablesDialog)
  , m_organizerCore(oc)
  , m_originalExecutables(*oc.executablesList())
  , m_executablesList(*oc.executablesList())
  , m_settingUI(false)
{
  ui->setupUi(this);
  ui->splitter->setSizes({200, 1});
  ui->splitter->setStretchFactor(0, 0);
  ui->splitter->setStretchFactor(1, 1);

  loadCustomOverwrites();
  loadForcedLibraries();

  ui->mods->addItems(m_organizerCore.modList()->allMods());
  fillList();
  setDirty(false);

  // some widgets need to do more than just save() and have their own handler
  connect(ui->binary, &QLineEdit::textChanged, [&]{ save(); });
  connect(ui->workingDirectory, &QLineEdit::textChanged, [&]{ save(); });
  connect(ui->arguments, &QLineEdit::textChanged, [&]{ save(); });
  connect(ui->steamAppID, &QLineEdit::textChanged, [&]{ save(); });
  connect(ui->mods, &QComboBox::currentTextChanged, [&]{ save(); });
  connect(ui->useApplicationIcon, &QCheckBox::toggled, [&]{ save(); });
  connect(ui->list->model(), &QAbstractItemModel::rowsMoved, [&]{ saveOrder(); });
}

EditExecutablesDialog::~EditExecutablesDialog() = default;

int EditExecutablesDialog::exec()
{
  GeometrySaver gs(Settings::instance(), this);
  return QDialog::exec();
}

void EditExecutablesDialog::loadCustomOverwrites()
{
  const auto* p = m_organizerCore.currentProfile();

  for (const auto& e : m_executablesList) {
    const auto s = p->setting("custom_overwrites", e.title()).toString();

    if (!s.isEmpty()) {
      m_customOverwrites.set(e.title(), true, s);
    }
  }
}

void EditExecutablesDialog::loadForcedLibraries()
{
  const auto* p = m_organizerCore.currentProfile();

  for (const auto& e : m_executablesList) {
    if (p->forcedLibrariesEnabled(e.title())) {
      m_forcedLibraries.set(e.title(), true, p->determineForcedLibraries(e.title()));
    }
  }
}

ExecutablesList EditExecutablesDialog::getExecutablesList() const
{
  ExecutablesList newList;

  // make sure the executables are in the same order as in the list
  for (int i = 0; i < ui->list->count(); ++i) {
    const auto& title = ui->list->item(i)->text();

    auto itor = m_executablesList.find(title);

    if (itor == m_executablesList.end()) {
      log::warn("getExecutablesList(): executable '{}' not found", title);
      continue;
    }

    newList.setExecutable(*itor);
  }

  return newList;
}

const EditExecutablesDialog::CustomOverwrites&
EditExecutablesDialog::getCustomOverwrites() const
{
  return m_customOverwrites;
}

const EditExecutablesDialog::ForcedLibraries&
EditExecutablesDialog::getForcedLibraries() const
{
  return m_forcedLibraries;
}

void EditExecutablesDialog::commitChanges()
{
  const auto newExecutables = getExecutablesList();
  auto* profile = m_organizerCore.currentProfile();

  // remove all the custom overwrites and forced libraries
  for (const auto& e : m_originalExecutables) {
    profile->removeSetting("custom_overwrites", e.title());
    profile->removeForcedLibraries(e.title());
  }

  // set the new custom overwrites and forced libraries
  for (const auto& e : newExecutables) {
    if (auto modName=m_customOverwrites.find(e.title())) {
      if (modName && modName->enabled) {
        profile->storeSetting("custom_overwrites", e.title(), modName->value);
      }
    }

    if (auto libraryList=m_forcedLibraries.find(e.title())) {
      if (libraryList && libraryList->enabled && !libraryList->value.empty()) {
        profile->setForcedLibrariesEnabled(e.title(), true);
        profile->storeForcedLibraries(e.title(), libraryList->value);
      }
    }
  }

  // set the new executables list
  m_organizerCore.setExecutablesList(newExecutables);

  setDirty(false);
}

void EditExecutablesDialog::setDirty(bool b)
{
  if (auto* button=ui->buttons->button(QDialogButtonBox::Apply)) {
    button->setEnabled(b);
  }
}

QListWidgetItem* EditExecutablesDialog::selectedItem()
{
  const auto selection = ui->list->selectedItems();

  if (selection.empty()) {
    return nullptr;
  }

  return selection[0];
}

Executable* EditExecutablesDialog::selectedExe()
{
  auto* item = selectedItem();
  if (!item) {
    return nullptr;
  }

  const auto& title = item->text();
  auto itor = m_executablesList.find(title);

  if (itor == m_executablesList.end()) {
    return nullptr;
  }

  return &*itor;
}

void EditExecutablesDialog::fillList()
{
  ui->list->clear();

  for(const auto& exe : m_executablesList) {
    ui->list->addItem(createListItem(exe));
  }

  // select the first one in the list, if any
  if (ui->list->count() > 0) {
    ui->list->item(0)->setSelected(true);
  } else {
    updateUI(nullptr, nullptr);
  }
}

QListWidgetItem* EditExecutablesDialog::createListItem(const Executable& exe)
{
  return new QListWidgetItem(exe.title());
}

void EditExecutablesDialog::updateUI(
  const QListWidgetItem* item, const Executable* e)
{
  // the ui is currently being set, ignore changes
  m_settingUI = true;

  if (e) {
    setEdits(*e);
  } else {
    clearEdits();
  }

  setButtons(item, e);

  // any changes from now on are from the user
  m_settingUI = false;
}

void EditExecutablesDialog::setButtons(
  const QListWidgetItem* item, const Executable* e)
{
  // add and remove are always enabled

  if (item) {
    ui->up->setEnabled(canMove(item, -1));
    ui->down->setEnabled(canMove(item, +1));
  } else {
    ui->up->setEnabled(false);
    ui->down->setEnabled(false);
  }
}

void EditExecutablesDialog::clearEdits()
{
  ui->title->clear();
  ui->title->setEnabled(false);
  ui->binary->clear();
  ui->binary->setEnabled(false);
  ui->browseBinary->setEnabled(false);
  ui->workingDirectory->clear();
  ui->workingDirectory->setEnabled(false);
  ui->browseWorkingDirectory->setEnabled(false);
  ui->arguments->clear();
  ui->arguments->setEnabled(false);
  ui->overwriteSteamAppID->setEnabled(false);
  ui->overwriteSteamAppID->setChecked(false);
  ui->steamAppID->setEnabled(false);
  ui->steamAppID->clear();
  ui->createFilesInMod->setEnabled(false);
  ui->createFilesInMod->setChecked(false);
  ui->mods->setEnabled(false);
  ui->mods->setCurrentIndex(-1);
  ui->forceLoadLibraries->setEnabled(false);
  ui->forceLoadLibraries->setChecked(false);
  ui->configureLibraries->setEnabled(false);
  ui->useApplicationIcon->setEnabled(false);
  ui->useApplicationIcon->setChecked(false);
}

void EditExecutablesDialog::setEdits(const Executable& e)
{
  ui->title->setText(e.title());
  ui->binary->setText(QDir::toNativeSeparators(e.binaryInfo().absoluteFilePath()));
  ui->workingDirectory->setText(QDir::toNativeSeparators(e.workingDirectory()));
  ui->arguments->setText(e.arguments());
  ui->overwriteSteamAppID->setChecked(!e.steamAppID().isEmpty());
  ui->steamAppID->setEnabled(!e.steamAppID().isEmpty());
  ui->steamAppID->setText(e.steamAppID());
  ui->useApplicationIcon->setChecked(e.usesOwnIcon());

  {
    int modIndex = -1;

    const auto modName = m_customOverwrites.find(e.title());

    if (modName && !modName->value.isEmpty()) {
      modIndex = ui->mods->findText(modName->value);

      if (modIndex == -1) {
        log::warn(
          "executable '{}' uses mod '{}' as a custom overwrite, but that mod "
          "doesn't exist",
          e.title(), modName->value);
      }
    }

    const bool hasCustomOverwrites = (modName && modName->enabled);

    ui->createFilesInMod->setChecked(hasCustomOverwrites);
    ui->mods->setEnabled(hasCustomOverwrites);
    ui->mods->setCurrentIndex(modIndex);
  }

  {
    const auto libraryList = m_forcedLibraries.find(e.title());
    const bool hasForcedLibraries = (libraryList && libraryList->enabled);

    ui->forceLoadLibraries->setChecked(hasForcedLibraries);
    ui->configureLibraries->setEnabled(hasForcedLibraries);
  }

  // always enabled
  ui->title->setEnabled(true);
  ui->binary->setEnabled(true);
  ui->browseBinary->setEnabled(true);
  ui->workingDirectory->setEnabled(true);
  ui->browseWorkingDirectory->setEnabled(true);
  ui->arguments->setEnabled(true);
  ui->overwriteSteamAppID->setEnabled(true);
  ui->useApplicationIcon->setEnabled(true);
  ui->createFilesInMod->setEnabled(true);
  ui->forceLoadLibraries->setEnabled(true);
}

void EditExecutablesDialog::save()
{
  if (m_settingUI) {
    return;
  }

  auto* e = selectedExe();
  if (!e) {
    log::warn("trying to save but nothing is selected");
    return;
  }

  // title may have changed, start with the stuff using it

  // custom overwrites
  if (ui->createFilesInMod->isChecked()) {
    m_customOverwrites.set(e->title(), true, ui->mods->currentText());
  } else {
    m_customOverwrites.setEnabled(e->title(), false);
  }

  // forced libraries
  m_forcedLibraries.setEnabled(e->title(), ui->forceLoadLibraries->isChecked());

  // get the new title, but ignore it if it's conflicting with an already
  // existing executable
  QString newTitle = ui->title->text();
  if (isTitleConflicting(newTitle)) {
    newTitle = e->title();
  }

  if (e->title() != newTitle) {
    // now rename both the custom overwrites and forced libraries if the title
    // is being changed
    m_customOverwrites.rename(e->title(), newTitle);
    m_forcedLibraries.rename(e->title(), newTitle);

    // save the new title
    e->title(newTitle);
  }

  e->binaryInfo(ui->binary->text());
  e->workingDirectory(ui->workingDirectory->text());
  e->arguments(ui->arguments->text());

  if (ui->overwriteSteamAppID->isChecked()) {
    e->steamAppID(ui->steamAppID->text());
  } else {
    e->steamAppID("");
  }

  if (ui->useApplicationIcon->isChecked()) {
    e->flags(e->flags() | Executable::UseApplicationIcon);
  } else {
    e->flags(e->flags() & (~Executable::UseApplicationIcon));
  }

  setDirty(true);
}

void EditExecutablesDialog::saveOrder()
{
  m_executablesList = getExecutablesList();
  setDirty(true);
}

bool EditExecutablesDialog::canMove(const QListWidgetItem* item, int direction)
{
  if (!item) {
    return false;
  }

  if (direction < 0) {
    // moving up
    return (ui->list->row(item) > 0);

  } else if (direction > 0) {
    // moving down
    return (ui->list->row(item) < (ui->list->count() - 1));
  }

  return false;
}

void EditExecutablesDialog::move(QListWidgetItem* item, int direction)
{
  if (!canMove(item, direction)) {
    return;
  }

  const auto row = ui->list->row(item);

  // removing item
  ui->list->takeItem(row);
  ui->list->insertItem(row + (direction > 0 ? 1 : -1), item);
  item->setSelected(true);

  setDirty(true);
}

void EditExecutablesDialog::on_list_itemSelectionChanged()
{
  updateUI(selectedItem(), selectedExe());
}

void EditExecutablesDialog::on_reset_clicked()
{
  const auto title = tr("Reset plugin executables");

  const auto text = tr(
    "This will restore all the executables provided by the game plugin. If "
    "there are existing executables with the same names, they will be "
    "automatically renamed and left unchanged.");

  const auto buttons = QMessageBox::Ok | QMessageBox::Cancel;

  if (QMessageBox::question(this, title, text, buttons) != QMessageBox::Ok) {
    return;
  }

  m_executablesList.resetFromPlugin(m_organizerCore.managedGame());
  fillList();

  setDirty(true);
}

void EditExecutablesDialog::on_add_clicked()
{
  auto title = m_executablesList.makeNonConflictingTitle(tr("New Executable"));
  if (!title) {
    return;
  }

  const Executable e(*title);

  m_executablesList.setExecutable(e);

  auto* item = createListItem(e);
  ui->list->addItem(item);
  item->setSelected(true);

  setDirty(true);
}

void EditExecutablesDialog::on_remove_clicked()
{
  auto* item = selectedItem();
  if (!item) {
    log::warn("trying to remove entry but nothing is selected");
    return;
  }

  auto* exe = selectedExe();
  if (!exe) {
    log::warn("trying to remove entry but nothing is selected");
    return;
  }

  const int currentRow = ui->list->row(item);
  delete item;


  m_customOverwrites.remove(exe->title());
  m_forcedLibraries.remove(exe->title());

  // removing from main list, must be done last because it invalidates the
  // exe pointer
  m_executablesList.remove(exe->title());


  // reselecting the same row as before, or the last one
  if (currentRow >= ui->list->count()) {
    // that was the last item, select the new list item, if any
    if (ui->list->count() > 0) {
      ui->list->item(ui->list->count() - 1)->setSelected(true);
    }
  } else {
    ui->list->item(currentRow)->setSelected(true);
  }

  setDirty(true);
}

void EditExecutablesDialog::on_up_clicked()
{
  auto* item = selectedItem();
  if (!item) {
    return;
  }

  move(item, -1);
}

void EditExecutablesDialog::on_down_clicked()
{
  auto* item = selectedItem();
  if (!item) {
    return;
  }

  move(item, +1);
}

bool EditExecutablesDialog::isTitleConflicting(const QString& s)
{
  for (const auto& exe : m_executablesList) {
    if (exe.title() == s) {
      if (&exe != selectedExe()) {
        // found an executable that's not the current one with the same title
        return true;
      }
    }
  }

  return false;
}

void EditExecutablesDialog::on_title_textChanged(const QString& s)
{
  if (m_settingUI) {
    return;
  }

  // don't allow changing the title to something that already exists
  if (isTitleConflicting(s)) {
    return;
  }

  // must save before modifying the item in the list widget because saving
  // relies on the item's text being the same as an item in m_executablesList
  save();

  // once the executable is saved, the list item must be changed to match the
  // new name
  if (auto* i=selectedItem()) {
    i->setText(s);
  }
}

void EditExecutablesDialog::on_overwriteSteamAppID_toggled(bool checked)
{
  if (m_settingUI) {
    return;
  }

  ui->steamAppID->setEnabled(checked);
  save();
}

void EditExecutablesDialog::on_createFilesInMod_toggled(bool checked)
{
  if (m_settingUI) {
    return;
  }

  ui->mods->setEnabled(checked);
  save();
}

void EditExecutablesDialog::on_forceLoadLibraries_toggled(bool checked)
{
  if (m_settingUI) {
    return;
  }

  ui->configureLibraries->setEnabled(ui->forceLoadLibraries->isChecked());
  save();
}

void EditExecutablesDialog::on_browseBinary_clicked()
{
  const QString binaryName = FileDialogMemory::getOpenFileName(
    "editExecutableBinary", this, tr("Select a binary"), ui->binary->text(),
    tr("Executable (%1)").arg("*.exe *.bat *.jar"));

  if (binaryName.isNull()) {
    // canceled
    return;
  }

  // setting binary
  if (binaryName.endsWith(".jar", Qt::CaseInsensitive)) {
    // special case for jar files, uses the system java installation
    setJarBinary(binaryName);
  } else {
    ui->binary->setText(QDir::toNativeSeparators(binaryName));
  }

  // setting title if currently empty or some variation of "New Executable"
  if (ui->title->text().isEmpty() || 
      ui->title->text().startsWith(tr("New Executable"), Qt::CaseInsensitive)) {
    const auto prefix = QFileInfo(binaryName).baseName();
    const auto newTitle = m_executablesList.makeNonConflictingTitle(prefix);

    if (newTitle) {
      ui->title->setText(*newTitle);
    }
  }

  save();
}

void EditExecutablesDialog::on_browseWorkingDirectory_clicked()
{
  QString dirName = FileDialogMemory::getExistingDirectory(
    "editExecutableDirectory", this, tr("Select a directory"),
    ui->workingDirectory->text());

  if (dirName.isNull()) {
    // canceled
    return;
  }

  ui->workingDirectory->setText(dirName);
}

void EditExecutablesDialog::on_configureLibraries_clicked()
{
  auto* e = selectedExe();
  if (!e) {
    log::warn("trying to configure libraries but nothing is selected");
    return;
  }

  ForcedLoadDialog dialog(m_organizerCore.managedGame(), this);

  if (auto libraryList=m_forcedLibraries.find(e->title())) {
    dialog.setValues(libraryList->value);
  }

  if (dialog.exec() == QDialog::Accepted) {
    m_forcedLibraries.setValue(e->title(), dialog.values());
    save();
  }
}

void EditExecutablesDialog::on_buttons_clicked(QAbstractButton* b)
{
  if (b == ui->buttons->button(QDialogButtonBox::Ok)) {
    commitChanges();
    accept();
  } else if (b == ui->buttons->button(QDialogButtonBox::Apply)) {
    commitChanges();
  } else {
    reject();
  }
}

void EditExecutablesDialog::setJarBinary(const QString& binaryName)
{
  auto java = OrganizerCore::findJavaInstallation(binaryName);

  if (java.isEmpty()) {
    QMessageBox::information(
      this, tr("Java (32-bit) required"),
      tr("MO requires 32-bit java to run this application. If you already "
         "have it installed, select javaw.exe from that installation as "
         "the binary."));
  }

  // only save once

  m_settingUI = true;
  ui->binary->setText(java);
  ui->workingDirectory->setText(QDir::toNativeSeparators(QFileInfo(binaryName).absolutePath()));
  ui->arguments->setText("-jar \"" + QDir::toNativeSeparators(binaryName) + "\"");
  m_settingUI = false;

  save();
}
