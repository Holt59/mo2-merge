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

#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "settingsdialogdiagnostics.h"
#include "settingsdialoggeneral.h"
#include "settingsdialognexus.h"
#include "settingsdialogpaths.h"
#include "settingsdialogplugins.h"
#include "settingsdialogsteam.h"
#include "settingsdialogworkarounds.h"

using namespace MOBase;

SettingsDialog::SettingsDialog(PluginContainer *pluginContainer, Settings* settings, QWidget *parent)
  : TutorableDialog("SettingsDialog", parent)
  , ui(new Ui::SettingsDialog)
  , m_settings(settings)
  , m_PluginContainer(pluginContainer)
  , m_GeometriesReset(false)
  , m_keyChanged(false)
{
  ui->setupUi(this);

  m_tabs.push_back(std::unique_ptr<SettingsTab>(new GeneralSettingsTab(settings, *this)));
  m_tabs.push_back(std::unique_ptr<SettingsTab>(new PathsSettingsTab(settings, *this)));
  m_tabs.push_back(std::unique_ptr<SettingsTab>(new DiagnosticsSettingsTab(settings, *this)));
  m_tabs.push_back(std::unique_ptr<SettingsTab>(new NexusSettingsTab(settings, *this)));
  m_tabs.push_back(std::unique_ptr<SettingsTab>(new SteamSettingsTab(settings, *this)));
  m_tabs.push_back(std::unique_ptr<SettingsTab>(new PluginsSettingsTab(settings, *this)));
  m_tabs.push_back(std::unique_ptr<SettingsTab>(new WorkaroundsSettingsTab(settings, *this)));

  auto& qsettings = settings->directInterface();

  QString key = QString("geometry/%1").arg(objectName());
  if (qsettings.contains(key)) {
    restoreGeometry(qsettings.value(key).toByteArray());
  }
}

int SettingsDialog::exec()
{
  auto& qsettings = m_settings->directInterface();
  auto ret = TutorableDialog::exec();

  if (ret == QDialog::Accepted) {

    for (auto&& tab : m_tabs) {
      tab->closing();
    }

    // remember settings before change
    QMap<QString, QString> before;
    qsettings.beginGroup("Settings");
    for (auto k : qsettings.allKeys())
      before[k] = qsettings.value(k).toString();
    qsettings.endGroup();

    // transfer modified settings to configuration file
    for (std::unique_ptr<SettingsTab> const &tab: m_tabs) {
      tab->update();
    }

    // print "changed" settings
    qsettings.beginGroup("Settings");
    bool first_update = true;
    for (auto k : qsettings.allKeys())
      if (qsettings.value(k).toString() != before[k] && !k.contains("username") && !k.contains("password"))
      {
        if (first_update) {
          qDebug("Changed settings:");
          first_update = false;
        }
        qDebug("  %s=%s", k.toUtf8().data(), qsettings.value(k).toString().toUtf8().data());
      }
    qsettings.endGroup();
  }

  QString key = QString("geometry/%1").arg(objectName());
  qsettings.setValue(key, saveGeometry());

  // These changes happen regardless of accepted or rejected
  bool restartNeeded = false;
  if (getApiKeyChanged()) {
    restartNeeded = true;
  }
  if (getResetGeometries()) {
    restartNeeded = true;
    qsettings.setValue("reset_geometry", true);
  }
  if (restartNeeded) {
    if (QMessageBox::question(nullptr,
      tr("Restart Mod Organizer?"),
      tr("In order to finish configuration changes, MO must be restarted.\n"
        "Restart it now?"),
      QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      qApp->exit(INT_MAX);
    }
  }

  return ret;
}

SettingsDialog::~SettingsDialog()
{
  disconnect(this);
  delete ui;
}

QString SettingsDialog::getColoredButtonStyleSheet() const
{
  return QString("QPushButton {"
    "background-color: %1;"
    "color: %2;"
    "border: 1px solid;"
    "padding: 3px;"
    "}");
}

void SettingsDialog::accept()
{
  QString newModPath = ui->modDirEdit->text();
  newModPath.replace("%BASE_DIR%", ui->baseDirEdit->text());

  if ((QDir::fromNativeSeparators(newModPath) !=
       QDir::fromNativeSeparators(
           Settings::instance().getModDirectory(true))) &&
      (QMessageBox::question(
           nullptr, tr("Confirm"),
           tr("Changing the mod directory affects all your profiles! "
              "Mods not present (or named differently) in the new location "
              "will be disabled in all profiles. "
              "There is no way to undo this unless you backed up your "
              "profiles manually. Proceed?"),
           QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)) {
    return;
  }

  TutorableDialog::accept();
}

bool SettingsDialog::getResetGeometries()
{
  return ui->resetGeometryBtn->isChecked();
}

bool SettingsDialog::getApiKeyChanged()
{
  return m_keyChanged;
}


SettingsTab::SettingsTab(Settings *m_parent, SettingsDialog &m_dialog)
  : m_parent(m_parent)
  , m_Settings(m_parent->settingsRef())
  , m_dialog(m_dialog)
  , ui(m_dialog.ui)
{
}

SettingsTab::~SettingsTab()
{}

QWidget* SettingsTab::parentWidget()
{
  return &m_dialog;
}
