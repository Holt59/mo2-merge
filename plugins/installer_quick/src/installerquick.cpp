#include "installerquick.h"
#include "simpleinstalldialog.h"
#include <installationtester.h>

#include <QtPlugin>
#include <QDialog>

#include "log.h"


using namespace MOBase;


InstallerQuick::InstallerQuick()
  : m_MOInfo(nullptr)
{
}

bool InstallerQuick::init(IOrganizer *moInfo)
{
  m_MOInfo = moInfo;
  return true;
}

QString InstallerQuick::name() const
{
  return "Simple Installer";
}

QString InstallerQuick::author() const
{
  return "Tannin";
}

QString InstallerQuick::description() const
{
  return tr("Installer for very simple archives");
}

VersionInfo InstallerQuick::version() const
{
  return VersionInfo(1, 3, 0, VersionInfo::RELEASE_FINAL);
}

bool InstallerQuick::isActive() const
{
  return m_MOInfo->pluginSetting(name(), "enabled").toBool();
}

QList<PluginSetting> InstallerQuick::settings() const
{
  QList<PluginSetting> result;
  result.push_back(PluginSetting("enabled", "check to enable this plugin", QVariant(true)));
  result.push_back(PluginSetting("silent", "simple plugins will be installed without any user interaction", QVariant(false)));
  return result;
}

unsigned int InstallerQuick::priority() const
{
  return 50;
}


bool InstallerQuick::isManualInstaller() const
{
  return false;
}


bool InstallerQuick::isSimpleArchiveTopLayer(std::shared_ptr<const IFileTree> tree) const 
{
  static std::set<QString, MOBase::FileNameComparator> tlDirectoryNames = {
    "fonts", "interface", "menus", "meshes", "music", "scripts", "shaders",
    "sound", "strings", "textures", "trees", "video", "facegen", "materials",
    "skse", "obse", "mwse", "nvse", "fose", "f4se", "distantlod", "asi",
    "SkyProc Patchers", "Tools", "MCM", "icons", "bookart", "distantland",
    "mits", "splash", "dllplugins", "CalienteTools", "NetScriptFramework",
    "shadersfx"
  };
  static std::set<QString, MOBase::FileNameComparator> tlSuffixes = { 
    "esp", "esm", "esl", "bsa", "ba2", ".modgroups" };

  // see if there is at least one directory that makes sense on the top level
  for (auto entry : *tree) {
    if (entry->isDir() && tlDirectoryNames.count(entry->name()) > 0) {
      return true;
    }
    else if (entry->isFile() && tlSuffixes.count(entry->suffix()) > 0) {
      return true;
    }
  }

  return false;
}


bool InstallerQuick::isDataTextArchiveTopLayer(std::shared_ptr<const IFileTree> tree) const
{
  // A "DataText" archive is defined as having exactly one folder named data
  // and one or more text or PDF files (standard package from french modding site).
  static const std::set<QString, FileNameComparator> txtExtensions{ "txt", "pdf" };
  bool dataFound = false;
  bool txtFound = false;
  for (auto entry : *tree) {
    if (entry->isDir()) {
      // If data was already found, or this is a directory not named "data", fail:
      if (dataFound || entry->compare("data") != 0) {
        return false;
      }
      dataFound = true;
    }
    else {
      if (txtExtensions.count(entry->suffix()) == 0) {
        return false;
      }
      txtFound = true;
    }
  }
  return dataFound && txtFound;
}


std::shared_ptr<const IFileTree> InstallerQuick::getSimpleArchiveBase(std::shared_ptr<const IFileTree> dataTree) const
{
  while (true) {
    if (isSimpleArchiveTopLayer(dataTree) ||
        isDataTextArchiveTopLayer(dataTree)) {
      return dataTree;
    } else if (dataTree->size() == 1 && dataTree->at(0)->isDir()) {
      dataTree = dataTree->at(0)->astree();
    } else {
      log::debug("Archive is not a simple archive.");
      return nullptr;
    }
  }
}


bool InstallerQuick::isArchiveSupported(std::shared_ptr<const IFileTree> tree) const
{
  return getSimpleArchiveBase(tree) != nullptr;
}


IPluginInstaller::EInstallResult InstallerQuick::install(GuessedValue<QString> &modName, std::shared_ptr<IFileTree> &tree,
                                                         QString&, int&)
{
  tree = std::const_pointer_cast<IFileTree>(getSimpleArchiveBase(tree));
  if (tree != nullptr) {
    SimpleInstallDialog dialog(modName, parentWidget());
    if (m_MOInfo->pluginSetting(name(), "silent").toBool() || dialog.exec() == QDialog::Accepted) {
      modName.update(dialog.getName(), GUESS_USER);

      // If we have a data+txt archive, we move files to the data folder and
      // switch to the data folder:
      if (isDataTextArchiveTopLayer(tree)) {
        auto dataTree = tree->findDirectory("data");
        dataTree->detach();
        dataTree->merge(tree);
        tree = dataTree;
      }
      return RESULT_SUCCESS;
    } else {
      if (dialog.manualRequested()) {
        modName.update(dialog.getName(), GUESS_USER);
        return RESULT_MANUALREQUESTED;
      } else {
        return RESULT_CANCELED;
      }
    }
  } else {
    // install shouldn't even have even have been called
    qCritical("unsupported archive for quick installer");
    return RESULT_FAILED;
  }
}

#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
Q_EXPORT_PLUGIN2(installerQuick, InstallerQuick)
#endif
