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

#ifndef INSTALLERMANUAL_H
#define INSTALLERMANUAL_H


#include <iplugininstallersimple.h>


class InstallerManual : public IPluginInstallerSimple
{
  Q_OBJECT
  Q_INTERFACES(IPlugin IPluginInstaller IPluginInstallerSimple)

public:

  InstallerManual();

  virtual bool init(IOrganizer *moInfo);
  virtual QString name() const;
  virtual QString author() const;
  virtual QString description() const;
  virtual VersionInfo version() const;
  virtual bool isActive() const;
  virtual QList<PluginSetting> settings() const;

  virtual unsigned int priority() const;
  virtual bool isManualInstaller() const;

  virtual bool isArchiveSupported(const DirectoryTree &tree) const;
  virtual EInstallResult install(QString &modName, DirectoryTree &tree);

private:

  bool isSimpleArchiveTopLayer(const DirectoryTree::Node *node) const;
  const DirectoryTree::Node *getSimpleArchiveBase(const DirectoryTree &dataTree) const;

private slots:

  /**
   * @brief opens a file from the archive in the (system-)default editor/viewer
   * @param fileName relative name of the file to open
   */
  void openFile(const QString &fileName);

private:

  const IOrganizer *m_MOInfo;

};


#endif // INSTALLERMANUAL_H
