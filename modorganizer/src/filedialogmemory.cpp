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

#include "filedialogmemory.h"
#include "settings.h"
#include <QFileDialog>


FileDialogMemory::FileDialogMemory()
{
}


void FileDialogMemory::save(Settings& s)
{
  auto& settings = s.directInterface();

  settings.remove("recentDirectories");
  settings.beginWriteArray("recentDirectories");
  int index = 0;
  for (std::map<QString, QString>::const_iterator iter = instance().m_Cache.begin();
       iter != instance().m_Cache.end(); ++iter) {
    settings.setArrayIndex(index++);
    settings.setValue("name", iter->first);
    settings.setValue("directory", iter->second);
  }
  settings.endArray();
}


void FileDialogMemory::restore(const Settings& s)
{
  auto& settings = const_cast<QSettings&>(s.directInterface());

  int size = settings.beginReadArray("recentDirectories");
  for (int i = 0; i < size; ++i) {
    settings.setArrayIndex(i);
    QVariant name = settings.value("name");
    QVariant dir  = settings.value("directory");
    if (name.isValid() && dir.isValid()) {
      instance().m_Cache.insert(std::make_pair(name.toString(), dir.toString()));
    }
  }
  settings.endArray();
}


QString FileDialogMemory::getOpenFileName(
  const QString &dirID, QWidget *parent, const QString &caption,
  const QString &dir, const QString &filter, QString *selectedFilter,
  QFileDialog::Options options)
{
  QString currentDir = dir;

  if (currentDir.isEmpty()) {
    auto itor = instance().m_Cache.find(dirID);
    if (itor != instance().m_Cache.end()) {
      currentDir = itor->second;
    }
  }

  QString result = QFileDialog::getOpenFileName(
    parent, caption, currentDir, filter, selectedFilter, options);

  if (!result.isNull()) {
    instance().m_Cache[dirID] = QFileInfo(result).path();
  }

  return result;
}


QString FileDialogMemory::getExistingDirectory(
  const QString &dirID, QWidget *parent, const QString &caption,
  const QString &dir, QFileDialog::Options options)
{
  QString currentDir = dir;

  if (currentDir.isEmpty()) {
    auto itor = instance().m_Cache.find(dirID);
    if (itor != instance().m_Cache.end()) {
      currentDir = itor->second;
    }
  }

  QString result = QFileDialog::getExistingDirectory(
    parent, caption, currentDir, options);

  if (!result.isNull()) {
    instance().m_Cache[dirID] = QFileInfo(result).path();
  }

  return result;
}


FileDialogMemory &FileDialogMemory::instance()
{
  static FileDialogMemory instance;
  return instance;
}
