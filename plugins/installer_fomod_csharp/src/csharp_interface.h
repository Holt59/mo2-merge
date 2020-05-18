#ifndef CSHARP_INTERFACE_H
#define CSHARP_INTERFACE_H

#include "ifiletree.h"
#include "iplugininstaller.h"

namespace CSharp {

  void init(MOBase::IOrganizer* moInfo);

  void beforeInstall(MOBase::IInstallationManager* manager, QWidget* parentWidget, std::shared_ptr<MOBase::IFileTree> tree);

  std::shared_ptr<MOBase::IFileTree> afterInstall(bool success);

  MOBase::IPluginInstaller::EInstallResult executeCSharpScript(QString scriptPath);

}
#endif