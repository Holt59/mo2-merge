/*
Copyright (C) 2020 Holt59. All rights reserved.

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

#include "iinstallationmanager.h"

#include "installer_fomod_predialog.h"
#include "xml_info_reader.h"
#include "installer_fomod_csharp.h"
#include "csharp_interface.h"

using namespace MOBase;

bool InstallerFomodCSharp::init(IOrganizer *moInfo) {
  m_MOInfo = moInfo;
  CSharp::init(moInfo);
  return true;
}

std::shared_ptr<const IFileTree> InstallerFomodCSharp::findFomodDirectory(std::shared_ptr<const IFileTree> tree) const
{
  auto entry = tree->find("fomod", FileTreeEntry::DIRECTORY);

  if (entry != nullptr) {
    return entry->astree();
  }

  if (tree->size() == 1 && tree->at(0)->isDir()) {
    return findFomodDirectory(tree->at(0)->astree());
  }
  return nullptr;
}

std::shared_ptr<const FileTreeEntry> InstallerFomodCSharp::findScriptFile(std::shared_ptr<const IFileTree> tree) const {
  auto fomodDirectory = findFomodDirectory(tree);
  
  if (tree == nullptr) {
    return nullptr;
  }

  for (auto e : *fomodDirectory) {
    if (e->isFile() && e->suffix().compare("cs", Qt::CaseInsensitive) == 0) {
      return e;
    }
  }

  return nullptr;
}

std::shared_ptr<const FileTreeEntry> InstallerFomodCSharp::findInfoFile(std::shared_ptr<const IFileTree> tree) const {
  auto fomodDirectory = findFomodDirectory(tree);

  if (tree == nullptr) {
    return nullptr;
  }

  for (auto e : *fomodDirectory) {
    if (e->isFile() && e->compare("info.xml") == 0) {
      return e;
    }
  }

  return nullptr;
}

bool InstallerFomodCSharp::isArchiveSupported(std::shared_ptr<const MOBase::IFileTree> tree) const {
  return findScriptFile(tree) != nullptr;
}

InstallerFomodCSharp::EInstallResult InstallerFomodCSharp::install(MOBase::GuessedValue<QString>& modName, std::shared_ptr<MOBase::IFileTree>& tree,
  QString& version, int& modID) {

  // Extract the script file:
  auto scriptFile = findScriptFile(tree);
  if (scriptFile == nullptr) {
    return EInstallResult::RESULT_NOTATTEMPTED;
  }

  std::vector<std::shared_ptr<const FileTreeEntry>> toExtract{ scriptFile };

  // Check if there is a info.xml:
  auto infoFile = findInfoFile(tree);
  if (infoFile != nullptr) {
    toExtract.push_back(infoFile);
  }

  QStringList paths(manager()->extractFiles(toExtract));

  if (paths.size() == 2) {
    QFile file(paths[1]);
    if (file.open(QIODevice::ReadOnly)) {
      auto info = FomodInfoReader::readXml(file, &FomodInfoReader::parseInfo);
      if (!std::get<0>(info).isEmpty()) {
        modName.update(std::get<0>(info), GUESS_META);
      }
      if (std::get<1>(info) != -1) {
        modID = std::get<1>(info);
      }
      if (!std::get<2>(info).isEmpty()) {
        version = std::get<2>(info);
      }
    }
  }

  // Show the dialog:
  InstallerFomodPredialog dialog(modName, parentWidget());
  if (dialog.exec() != QDialog::Accepted) {
    if (dialog.nccRequested()) {
      modName.update(dialog.getName(), GUESS_USER);
      return EInstallResult::RESULT_NOTATTEMPTED;
    }
    else if (dialog.manualRequested()) {
      modName.update(dialog.getName(), GUESS_USER);
      return EInstallResult::RESULT_MANUALREQUESTED;
    }
    else {
      return EInstallResult::RESULT_CANCELED;
    }
  }
  modName.update(dialog.getName(), GUESS_USER);

  // Run the C# script:
  CSharp::beforeInstall(manager(), parentWidget(), tree);
  auto result = CSharp::executeCSharpScript(paths[0]);
  auto newTree = CSharp::afterInstall(result == EInstallResult::RESULT_SUCCESS);
  if (result == EInstallResult::RESULT_SUCCESS) {
    tree = newTree;
  }

  return result;
}