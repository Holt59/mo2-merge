#include "organizercore.h"

#include "delayedfilewriter.h"
#include "guessedvalue.h"
#include "imodinterface.h"
#include "imoinfo.h"
#include "iplugingame.h"
#include "iuserinterface.h"
#include "loadmechanism.h"
#include "messagedialog.h"
#include "modlistsortproxy.h"
#include "modrepositoryfileinfo.h"
#include "nexusinterface.h"
#include "plugincontainer.h"
#include "pluginlistsortproxy.h"
#include "profile.h"
#include "logbuffer.h"
#include "credentialsdialog.h"
#include "filedialogmemory.h"
#include "modinfodialog.h"
#include "spawn.h"
#include "syncoverwritedialog.h"
#include "nxmaccessmanager.h"
#include <ipluginmodpage.h>
#include <dataarchives.h>
#include <localsavegames.h>
#include <directoryentry.h>
#include <scopeguard.h>
#include <utility.h>
#include <usvfs.h>
#include "appconfig.h"
#include <report.h>
#include <questionboxmemory.h>
#include "lockeddialog.h"
#include "instancemanager.h"
#include <scriptextender.h>
#include "helper.h"
#include "previewdialog.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include <QWidget>

#include <QtDebug>
#include <QtGlobal> // for qUtf8Printable, etc

#include <Psapi.h>
#include <Shlobj.h>
#include <tlhelp32.h>
#include <tchar.h> // for _tcsicmp

#include <limits.h>
#include <stddef.h>
#include <string.h> // for memset, wcsrchr

#include <exception>
#include <functional>
#include <boost/algorithm/string/predicate.hpp>
#include <memory>
#include <set>
#include <string> //for wstring
#include <tuple>
#include <utility>


using namespace MOShared;
using namespace MOBase;

//static
CrashDumpsType OrganizerCore::m_globalCrashDumpsType = CrashDumpsType::None;

static bool isOnline()
{
  QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();

  bool connected = false;
  for (auto iter = interfaces.begin(); iter != interfaces.end() && !connected;
       ++iter) {
    if ((iter->flags() & QNetworkInterface::IsUp)
        && (iter->flags() & QNetworkInterface::IsRunning)
        && !(iter->flags() & QNetworkInterface::IsLoopBack)) {
      auto addresses = iter->addressEntries();
      if (addresses.count() == 0) {
        continue;
      }
      qDebug("interface %s seems to be up (address: %s)",
             qUtf8Printable(iter->humanReadableName()),
             qUtf8Printable(addresses[0].ip().toString()));
      connected = true;
    }
  }

  return connected;
}

static bool renameFile(const QString &oldName, const QString &newName,
                       bool overwrite = true)
{
  if (overwrite && QFile::exists(newName)) {
    QFile::remove(newName);
  }
  return QFile::rename(oldName, newName);
}

static std::wstring getProcessName(HANDLE process)
{
  wchar_t buffer[MAX_PATH];
  const wchar_t *fileName = L"unknown";

  if (process == nullptr) return fileName;

  if (::GetProcessImageFileNameW(process, buffer, MAX_PATH) != 0) {
    fileName = wcsrchr(buffer, L'\\');
    if (fileName == nullptr) {
      fileName = buffer;
    }
    else {
      fileName += 1;
    }
  }

  return fileName;
}

// Get parent PID for the given process, return 0 on failure
static DWORD getProcessParentID(DWORD pid)
{
  HANDLE th = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  PROCESSENTRY32 pe = { 0 };
  pe.dwSize = sizeof(PROCESSENTRY32);

  DWORD res = 0;
  if (Process32First(th, &pe))
    do {
      if (pe.th32ProcessID == pid) {
        res = pe.th32ParentProcessID;
        break;
      }
    } while (Process32Next(th, &pe));

  CloseHandle(th);

  return res;
}

static void startSteam(QWidget *widget)
{
  QSettings steamSettings("HKEY_CURRENT_USER\\Software\\Valve\\Steam",
                          QSettings::NativeFormat);
  QString exe = steamSettings.value("SteamExe", "").toString();
  if (!exe.isEmpty()) {
    exe = QString("\"%1\"").arg(exe);
    // See if username and password supplied. If so, pass them into steam.
    QStringList args;
    QString username;
    QString password;
    if (Settings::instance().getSteamLogin(username, password)) {
      args << "-login";
      args << username;
      if (password != "") {
        args << password;
      }
    }
    if (!QProcess::startDetached(exe, args)) {
      reportError(QObject::tr("Failed to start \"%1\"").arg(exe));
    } else {
      QMessageBox::information(
          widget, QObject::tr("Waiting"),
          QObject::tr("Please press OK once you're logged into steam."));
    }
  }
}

template <typename InputIterator>
QStringList toStringList(InputIterator current, InputIterator end)
{
  QStringList result;
  for (; current != end; ++current) {
    result.append(*current);
  }
  return result;
}

bool checkService()
{
  SC_HANDLE serviceManagerHandle = NULL;
  SC_HANDLE serviceHandle = NULL;
  LPSERVICE_STATUS_PROCESS serviceStatus = NULL;
  LPQUERY_SERVICE_CONFIG serviceConfig = NULL;
  bool serviceRunning = true;

  DWORD bytesNeeded;

  try {
    serviceManagerHandle = OpenSCManager(NULL, NULL, SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
    if (!serviceManagerHandle) {
      qWarning("failed to open service manager (query status) (error %d)", GetLastError());
      throw 1;
    }

    serviceHandle = OpenService(serviceManagerHandle, L"EventLog", SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
    if (!serviceHandle) {
      qWarning("failed to open EventLog service (query status) (error %d)", GetLastError());
      throw 2;
    }

    if (QueryServiceConfig(serviceHandle, NULL, 0, &bytesNeeded)
      || (GetLastError() != ERROR_INSUFFICIENT_BUFFER)) {
      qWarning("failed to get size of service config (error %d)", GetLastError());
      throw 3;
    }

    DWORD serviceConfigSize = bytesNeeded;
    serviceConfig = (LPQUERY_SERVICE_CONFIG)LocalAlloc(LMEM_FIXED, serviceConfigSize);
    if (!QueryServiceConfig(serviceHandle, serviceConfig, serviceConfigSize, &bytesNeeded)) {
      qWarning("failed to query service config (error %d)", GetLastError());
      throw 4;
    }

    if (serviceConfig->dwStartType == SERVICE_DISABLED) {
      qCritical("Windows Event Log service is disabled!");
      serviceRunning = false;
    }

    if (QueryServiceStatusEx(serviceHandle, SC_STATUS_PROCESS_INFO, NULL, 0, &bytesNeeded)
      || (GetLastError() != ERROR_INSUFFICIENT_BUFFER)) {
      qWarning("failed to get size of service status (error %d)", GetLastError());
      throw 5;
    }

    DWORD serviceStatusSize = bytesNeeded;
    serviceStatus = (LPSERVICE_STATUS_PROCESS)LocalAlloc(LMEM_FIXED, serviceStatusSize);
    if (!QueryServiceStatusEx(serviceHandle, SC_STATUS_PROCESS_INFO, (LPBYTE)serviceStatus, serviceStatusSize, &bytesNeeded)) {
      qWarning("failed to query service status (error %d)", GetLastError());
      throw 6;
    }

    if (serviceStatus->dwCurrentState != SERVICE_RUNNING) {
      qCritical("Windows Event Log service is not running");
      serviceRunning = false;
    }
  }
  catch (int e) {
    UNUSED_VAR(e);
    serviceRunning = false;
  }

  if (serviceStatus) {
    LocalFree(serviceStatus);
  }
  if (serviceConfig) {
    LocalFree(serviceConfig);
  }
  if (serviceHandle) {
    CloseServiceHandle(serviceHandle);
  }
  if (serviceManagerHandle) {
    CloseServiceHandle(serviceManagerHandle);
  }

  return serviceRunning;
}


OrganizerCore::OrganizerCore(const QSettings &initSettings)
  : m_UserInterface(nullptr)
  , m_PluginContainer(nullptr)
  , m_GameName()
  , m_CurrentProfile(nullptr)
  , m_Settings(initSettings)
  , m_Updater(NexusInterface::instance(m_PluginContainer))
  , m_AboutToRun()
  , m_FinishedRun()
  , m_ModInstalled()
  , m_ModList(m_PluginContainer, this)
  , m_PluginList(this)
  , m_DirectoryRefresher()
  , m_DirectoryStructure(new DirectoryEntry(L"data", nullptr, 0))
  , m_DownloadManager(NexusInterface::instance(m_PluginContainer), this)
  , m_InstallationManager()
  , m_RefresherThread()
  , m_DirectoryUpdate(false)
  , m_ArchivesInit(false)
  , m_PluginListsWriter(std::bind(&OrganizerCore::savePluginList, this))
{
  m_DownloadManager.setOutputDirectory(m_Settings.getDownloadDirectory());
  m_DownloadManager.setPreferredServers(m_Settings.getPreferredServers());

  NexusInterface::instance(m_PluginContainer)->setCacheDirectory(m_Settings.getCacheDirectory());

  MOBase::QuestionBoxMemory::init(initSettings.fileName());

  m_InstallationManager.setModsDirectory(m_Settings.getModDirectory());
  m_InstallationManager.setDownloadDirectory(m_Settings.getDownloadDirectory());

  connect(&m_DownloadManager, SIGNAL(downloadSpeed(QString, int)), this,
          SLOT(downloadSpeed(QString, int)));
  connect(&m_DirectoryRefresher, SIGNAL(refreshed()), this,
          SLOT(directory_refreshed()));

  connect(&m_ModList, SIGNAL(removeOrigin(QString)), this,
          SLOT(removeOrigin(QString)));

  connect(NexusInterface::instance(m_PluginContainer)->getAccessManager(),
          SIGNAL(validateSuccessful(bool)), this, SLOT(loginSuccessful(bool)));
  connect(NexusInterface::instance(m_PluginContainer)->getAccessManager(),
          SIGNAL(validateFailed(QString)), this, SLOT(loginFailed(QString)));

  // This seems awfully imperative
  connect(this, SIGNAL(managedGameChanged(MOBase::IPluginGame const *)),
          &m_Settings, SLOT(managedGameChanged(MOBase::IPluginGame const *)));
  connect(this, SIGNAL(managedGameChanged(MOBase::IPluginGame const *)),
          &m_DownloadManager,
          SLOT(managedGameChanged(MOBase::IPluginGame const *)));
  connect(this, SIGNAL(managedGameChanged(MOBase::IPluginGame const *)),
          &m_PluginList, SLOT(managedGameChanged(MOBase::IPluginGame const *)));

  connect(&m_PluginList, &PluginList::writePluginsList, &m_PluginListsWriter,
          &DelayedFileWriterBase::write);

  // make directory refresher run in a separate thread
  m_RefresherThread.start();
  m_DirectoryRefresher.moveToThread(&m_RefresherThread);
}

OrganizerCore::~OrganizerCore()
{
  m_RefresherThread.exit();
  m_RefresherThread.wait();

  prepareStart();

  // profile has to be cleaned up before the modinfo-buffer is cleared
  delete m_CurrentProfile;
  m_CurrentProfile = nullptr;

  ModInfo::clear();
  LogBuffer::cleanQuit();
  m_ModList.setProfile(nullptr);
  //  NexusInterface::instance()->cleanup();

  delete m_DirectoryStructure;
}

QString OrganizerCore::commitSettings(const QString &iniFile)
{
  if (!shellRename(iniFile + ".new", iniFile, true, qApp->activeWindow())) {
    DWORD err = ::GetLastError();
    // make a second attempt using qt functions but if that fails print the
    // error from the first attempt
    if (!renameFile(iniFile + ".new", iniFile)) {
      return windowsErrorString(err);
    }
  }
  return QString();
}

QSettings::Status OrganizerCore::storeSettings(const QString &fileName)
{
  QSettings settings(fileName, QSettings::IniFormat);

  if (m_UserInterface != nullptr) {
    m_UserInterface->storeSettings(settings);
  }

  if (m_CurrentProfile != nullptr) {
    settings.setValue("selected_profile",
                      m_CurrentProfile->name().toUtf8().constData());
  }

  m_ExecutablesList.store(settings);

  FileDialogMemory::save(settings);

  settings.sync();
  return settings.status();
}

void OrganizerCore::storeSettings()
{
  QString iniFile = qApp->property("dataPath").toString() + "/"
                    + QString::fromStdWString(AppConfig::iniFileName());
  if (QFileInfo(iniFile).exists()) {
    if (!shellCopy(iniFile, iniFile + ".new", true, qApp->activeWindow())) {
      QMessageBox::critical(
          qApp->activeWindow(), tr("Failed to write settings"),
          tr("An error occurred trying to update MO settings to %1: %2")
              .arg(iniFile, windowsErrorString(::GetLastError())));
      return;
    }
  }

  QString writeTarget = iniFile + ".new";

  QSettings::Status result = storeSettings(writeTarget);

  if (result == QSettings::NoError) {
    QString errMsg = commitSettings(iniFile);
    if (!errMsg.isEmpty()) {
      qWarning("settings file not writable, may be locked by another "
               "application, trying direct write");
      writeTarget = iniFile;
      result = storeSettings(iniFile);
    }
  }
  if (result != QSettings::NoError) {
    QString reason = result == QSettings::AccessError
                         ? tr("File is write protected")
                         : result == QSettings::FormatError
                               ? tr("Invalid file format (probably a bug)")
                               : tr("Unknown error %1").arg(result);
    QMessageBox::critical(
        qApp->activeWindow(), tr("Failed to write settings"),
        tr("An error occurred trying to write back MO settings to %1: %2")
            .arg(writeTarget, reason));
  }
}

bool OrganizerCore::testForSteam(bool *found, bool *access)
{
  HANDLE hProcessSnap;
  HANDLE hProcess;
  PROCESSENTRY32 pe32;
  DWORD lastError;

  if (found == nullptr || access == nullptr) {
    return false;
  }

  // Take a snapshot of all processes in the system.
  hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hProcessSnap == INVALID_HANDLE_VALUE) {
    lastError = GetLastError();
    qCritical("unable to get snapshot of processes (error %d)", lastError);
    return false;
  }

  // Retrieve information about the first process,
  // and exit if unsuccessful
  pe32.dwSize = sizeof(PROCESSENTRY32);
  if (!Process32First(hProcessSnap, &pe32)) {
    lastError = GetLastError();
    qCritical("unable to get first process (error %d)", lastError);
    CloseHandle(hProcessSnap);
    return false;
  }

  *found = false;
  *access = true;

  // Now walk the snapshot of processes, and
  // display information about each process in turn
  do {
    if ((_tcsicmp(pe32.szExeFile, L"Steam.exe") == 0) ||
        (_tcsicmp(pe32.szExeFile, L"SteamService.exe") == 0)) {

      *found = true;

      // Try to open the process to determine if MO has the proper access
      hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                             FALSE, pe32.th32ProcessID);
      if (hProcess == NULL) {
        lastError = GetLastError();
        if (lastError == ERROR_ACCESS_DENIED) {
          *access = false;
        }
      } else {
        CloseHandle(hProcess);
      }
      break;
    }

} while(Process32Next(hProcessSnap, &pe32));

CloseHandle(hProcessSnap);
return true;

}

void OrganizerCore::updateExecutablesList(QSettings &settings)
{
  if (m_PluginContainer == nullptr) {
    qCritical("can't update executables list now");
    return;
  }

  m_ExecutablesList.load(managedGame(), settings);

  // TODO this has nothing to do with executables list move to an appropriate
  // function!
  ModInfo::updateFromDisc(m_Settings.getModDirectory(), &m_DirectoryStructure,
                          m_PluginContainer, m_Settings.displayForeign(), managedGame());
}

void OrganizerCore::setUserInterface(IUserInterface *userInterface,
                                     QWidget *widget)
{
  storeSettings();

  m_UserInterface = userInterface;

  if (widget != nullptr) {
    connect(&m_ModList, SIGNAL(modlistChanged(QModelIndex, int)), widget,
            SLOT(modlistChanged(QModelIndex, int)));
    connect(&m_ModList, SIGNAL(modlistChanged(QModelIndexList, int)), widget,
            SLOT(modlistChanged(QModelIndexList, int)));
    connect(&m_ModList, SIGNAL(showMessage(QString)), widget,
            SLOT(showMessage(QString)));
    connect(&m_ModList, SIGNAL(modRenamed(QString, QString)), widget,
            SLOT(modRenamed(QString, QString)));
    connect(&m_ModList, SIGNAL(modUninstalled(QString)), widget,
            SLOT(modRemoved(QString)));
    connect(&m_ModList, SIGNAL(removeSelectedMods()), widget,
            SLOT(removeMod_clicked()));
    connect(&m_ModList, SIGNAL(clearOverwrite()), widget,
      SLOT(clearOverwrite()));
    connect(&m_ModList, SIGNAL(requestColumnSelect(QPoint)), widget,
            SLOT(displayColumnSelection(QPoint)));
    connect(&m_ModList, SIGNAL(fileMoved(QString, QString, QString)), widget,
            SLOT(fileMoved(QString, QString, QString)));
    connect(&m_ModList, SIGNAL(modorder_changed()), widget,
            SLOT(modorder_changed()));
    connect(&m_PluginList, SIGNAL(writePluginsList()), widget,
      SLOT(esplist_changed()));
    connect(&m_PluginList, SIGNAL(esplist_changed()), widget,
      SLOT(esplist_changed()));
    connect(&m_DownloadManager, SIGNAL(showMessage(QString)), widget,
            SLOT(showMessage(QString)));
  }

  m_InstallationManager.setParentWidget(widget);
  m_Updater.setUserInterface(widget);

  if (userInterface != nullptr) {
    // this currently wouldn't work reliably if the ui isn't initialized yet to
    // display the result
    if (isOnline() && !m_Settings.offlineMode()) {
      m_Updater.testForUpdate();
    } else {
      qDebug("user doesn't seem to be connected to the internet");
    }
  }
}

void OrganizerCore::connectPlugins(PluginContainer *container)
{
  m_DownloadManager.setSupportedExtensions(
      m_InstallationManager.getSupportedExtensions());
  m_PluginContainer = container;
  m_Updater.setPluginContainer(m_PluginContainer);
  m_DownloadManager.setPluginContainer(m_PluginContainer);
  m_ModList.setPluginContainer(m_PluginContainer);

  if (!m_GameName.isEmpty()) {
    m_GamePlugin = m_PluginContainer->managedGame(m_GameName);
    emit managedGameChanged(m_GamePlugin);
  }
}

void OrganizerCore::disconnectPlugins()
{
  m_AboutToRun.disconnect_all_slots();
  m_FinishedRun.disconnect_all_slots();
  m_ModInstalled.disconnect_all_slots();
  m_ModList.disconnectSlots();
  m_PluginList.disconnectSlots();
  m_Updater.setPluginContainer(nullptr);
  m_DownloadManager.setPluginContainer(nullptr);
  m_ModList.setPluginContainer(nullptr);

  m_Settings.clearPlugins();
  m_GamePlugin      = nullptr;
  m_PluginContainer = nullptr;
}

void OrganizerCore::setManagedGame(MOBase::IPluginGame *game)
{
  m_GameName   = game->gameName();
  m_GamePlugin = game;
  qApp->setProperty("managed_game", QVariant::fromValue(m_GamePlugin));
  emit managedGameChanged(m_GamePlugin);
}

Settings &OrganizerCore::settings()
{
  return m_Settings;
}

bool OrganizerCore::nexusApi(bool retry)
{
  NXMAccessManager *accessManager
      = NexusInterface::instance(m_PluginContainer)->getAccessManager();

  if ((accessManager->validateAttempted() || accessManager->validated())
      && !retry) {
    // previous attempt, maybe even successful
    return false;
  } else {
    QString apiKey;
    if (m_Settings.getNexusApiKey(apiKey)) {
      // credentials stored or user entered them manually
      qDebug("attempt to verify nexus api key");
      accessManager->apiCheck(apiKey);
      return true;
    } else {
      // no credentials stored and user didn't enter them
      accessManager->refuseValidation();
      return false;
    }
  }
}

void OrganizerCore::startMOUpdate()
{
  if (nexusApi()) {
    m_PostLoginTasks.append([&]() { m_Updater.startUpdate(); });
  } else {
    m_Updater.startUpdate();
  }
}

void OrganizerCore::downloadRequestedNXM(const QString &url)
{
  qDebug("download requested: %s", qUtf8Printable(url));
  if (nexusApi()) {
    m_PendingDownloads.append(url);
  } else {
    m_DownloadManager.addNXMDownload(url);
  }
}

void OrganizerCore::externalMessage(const QString &message)
{
  if (MOShortcut moshortcut{ message } ) {
    if(moshortcut.hasExecutable())
      runShortcut(moshortcut);
  }
  else if (isNxmLink(message)) {
    MessageDialog::showMessage(tr("Download started"), qApp->activeWindow());
    downloadRequestedNXM(message);
  }
}

void OrganizerCore::downloadRequested(QNetworkReply *reply, QString gameName, int modID,
                                      const QString &fileName)
{
  try {
    if (m_DownloadManager.addDownload(reply, QStringList(), fileName, gameName, modID, 0,
                                      new ModRepositoryFileInfo(gameName, modID))) {
      MessageDialog::showMessage(tr("Download started"), qApp->activeWindow());
    }
  } catch (const std::exception &e) {
    MessageDialog::showMessage(tr("Download failed"), qApp->activeWindow());
    qCritical("exception starting download: %s", e.what());
  }
}

void OrganizerCore::removeOrigin(const QString &name)
{
  FilesOrigin &origin = m_DirectoryStructure->getOriginByName(ToWString(name));
  origin.enable(false);
  refreshLists();
}

void OrganizerCore::downloadSpeed(const QString &serverName, int bytesPerSecond)
{
  m_Settings.setDownloadSpeed(serverName, bytesPerSecond);
}

InstallationManager *OrganizerCore::installationManager()
{
  return &m_InstallationManager;
}

bool OrganizerCore::createDirectory(const QString &path) {
  if (!QDir(path).exists() && !QDir().mkpath(path)) {
    QMessageBox::critical(nullptr, QObject::tr("Error"),
                          QObject::tr("Failed to create \"%1\". Your user "
                                      "account probably lacks permission.")
                              .arg(QDir::toNativeSeparators(path)));
    return false;
  } else {
    return true;
  }
}

bool OrganizerCore::checkPathSymlinks() {
  bool hasSymlink = (QFileInfo(m_Settings.getProfileDirectory()).isSymLink() ||
    QFileInfo(m_Settings.getModDirectory()).isSymLink() ||
    QFileInfo(m_Settings.getOverwriteDirectory()).isSymLink());
  if (hasSymlink) {
    QMessageBox::critical(nullptr, QObject::tr("Error"),
      QObject::tr("One of the configured MO2 directories (profiles, mods, or overwrite) "
        "is on a path containing a symbolic (or other) link. This is incompatible "
        "with MO2's VFS system."));
    return false;
  }
  return true;
}

bool OrganizerCore::bootstrap() {
  return createDirectory(m_Settings.getProfileDirectory()) &&
         createDirectory(m_Settings.getModDirectory()) &&
         createDirectory(m_Settings.getDownloadDirectory()) &&
         createDirectory(m_Settings.getOverwriteDirectory()) &&
         createDirectory(QString::fromStdWString(crashDumpsPath())) &&
         checkPathSymlinks() && cycleDiagnostics();
}

void OrganizerCore::createDefaultProfile()
{
  QString profilesPath = settings().getProfileDirectory();
  if (QDir(profilesPath).entryList(QDir::AllDirs | QDir::NoDotAndDotDot).size()
      == 0) {
    Profile newProf("Default", managedGame(), false);
  }
}

void OrganizerCore::prepareVFS()
{
  m_USVFS.updateMapping(fileMapping(m_CurrentProfile->name(), QString()));
}

void OrganizerCore::updateVFSParams(int logLevel, int crashDumpsType, QString executableBlacklist) {
  setGlobalCrashDumpsType(crashDumpsType);
  m_USVFS.updateParams(logLevel, crashDumpsType, executableBlacklist);
}

bool OrganizerCore::cycleDiagnostics() {
  if (int maxDumps = settings().crashDumpsMax())
    removeOldFiles(QString::fromStdWString(crashDumpsPath()), "*.dmp", maxDumps, QDir::Time|QDir::Reversed);
  return true;
}

//static
void OrganizerCore::setGlobalCrashDumpsType(int crashDumpsType) {
  m_globalCrashDumpsType = ::crashDumpsType(crashDumpsType);
}

//static
std::wstring OrganizerCore::crashDumpsPath() {
  return (
    qApp->property("dataPath").toString() + "/"
    + QString::fromStdWString(AppConfig::dumpsDir())
    ).toStdWString();
}

bool OrganizerCore::getArchiveParsing() const
{
  return m_ArchiveParsing;
}

void OrganizerCore::setArchiveParsing(const bool archiveParsing)
{
  m_ArchiveParsing = archiveParsing;
}

void OrganizerCore::setCurrentProfile(const QString &profileName)
{
  if ((m_CurrentProfile != nullptr)
      && (profileName == m_CurrentProfile->name())) {
    return;
  }

  QDir profileBaseDir(settings().getProfileDirectory());
  QString profileDir = profileBaseDir.absoluteFilePath(profileName);

  if (!QDir(profileDir).exists()) {
    // selected profile doesn't exist. Ensure there is at least one profile,
    // then pick any one
    createDefaultProfile();

    profileDir = profileBaseDir.absoluteFilePath(
        profileBaseDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot).at(0));
  }

  Profile *newProfile = new Profile(QDir(profileDir), managedGame());

  delete m_CurrentProfile;
  m_CurrentProfile = newProfile;
  m_ModList.setProfile(newProfile);

  if (m_CurrentProfile->invalidationActive(nullptr)) {
    m_CurrentProfile->activateInvalidation();
  } else {
    m_CurrentProfile->deactivateInvalidation();
  }

  connect(m_CurrentProfile, SIGNAL(modStatusChanged(uint)), this, SLOT(modStatusChanged(uint)));
  connect(m_CurrentProfile, SIGNAL(modStatusChanged(QList<uint>)), this, SLOT(modStatusChanged(QList<uint>)));
  refreshDirectoryStructure();

  //This line is not actually needed and was only added to allow some
  //outside detection of Mo2 profile change. (like BaobobMiller utility)
  if (m_CurrentProfile != nullptr) {
    settings().directInterface().setValue("selected_profile",
      m_CurrentProfile->name().toUtf8().constData());
  }
}

MOBase::IModRepositoryBridge *OrganizerCore::createNexusBridge() const
{
  return new NexusBridge(m_PluginContainer);
}

QString OrganizerCore::profileName() const
{
  if (m_CurrentProfile != nullptr) {
    return m_CurrentProfile->name();
  } else {
    return "";
  }
}

QString OrganizerCore::profilePath() const
{
  if (m_CurrentProfile != nullptr) {
    return m_CurrentProfile->absolutePath();
  } else {
    return "";
  }
}

QString OrganizerCore::downloadsPath() const
{
  return QDir::fromNativeSeparators(m_Settings.getDownloadDirectory());
}

QString OrganizerCore::overwritePath() const
{
  return QDir::fromNativeSeparators(m_Settings.getOverwriteDirectory());
}

QString OrganizerCore::basePath() const
{
  return QDir::fromNativeSeparators(m_Settings.getBaseDirectory());
}

QString OrganizerCore::modsPath() const
{
  return QDir::fromNativeSeparators(m_Settings.getModDirectory());
}

MOBase::VersionInfo OrganizerCore::appVersion() const
{
  return m_Updater.getVersion();
}

MOBase::IModInterface *OrganizerCore::getMod(const QString &name) const
{
  unsigned int index = ModInfo::getIndex(name);
  return index == UINT_MAX ? nullptr : ModInfo::getByIndex(index).data();
}

MOBase::IPluginGame *OrganizerCore::getGame(const QString &name) const
{
  for (IPluginGame *game : m_PluginContainer->plugins<IPluginGame>()) {
    if (game != nullptr && game->gameShortName().compare(name, Qt::CaseInsensitive) == 0)
      return game;
  }
  return nullptr;
}

MOBase::IModInterface *OrganizerCore::createMod(GuessedValue<QString> &name)
{
  bool merge = false;
  if (!m_InstallationManager.testOverwrite(name, &merge)) {
    return nullptr;
  }

  m_InstallationManager.setModsDirectory(m_Settings.getModDirectory());

  QString targetDirectory
      = QDir::fromNativeSeparators(m_Settings.getModDirectory())
            .append("/")
            .append(name);

  QSettings settingsFile(targetDirectory + "/meta.ini", QSettings::IniFormat);

  if (!merge) {
    settingsFile.setValue("modid", 0);
    settingsFile.setValue("version", "");
    settingsFile.setValue("newestVersion", "");
    settingsFile.setValue("category", 0);
    settingsFile.setValue("installationFile", "");

    settingsFile.remove("installedFiles");
    settingsFile.beginWriteArray("installedFiles", 0);
    settingsFile.endArray();
  }

  return ModInfo::createFrom(m_PluginContainer, m_GamePlugin, QDir(targetDirectory), &m_DirectoryStructure)
      .data();
}

bool OrganizerCore::removeMod(MOBase::IModInterface *mod)
{
  unsigned int index = ModInfo::getIndex(mod->name());
  if (index == UINT_MAX) {
    return mod->remove();
  } else {
    return ModInfo::removeMod(index);
  }
}

void OrganizerCore::modDataChanged(MOBase::IModInterface *)
{
  refreshModList(false);
}

QVariant OrganizerCore::pluginSetting(const QString &pluginName,
                                      const QString &key) const
{
  return m_Settings.pluginSetting(pluginName, key);
}

void OrganizerCore::setPluginSetting(const QString &pluginName,
                                     const QString &key, const QVariant &value)
{
  m_Settings.setPluginSetting(pluginName, key, value);
}

QVariant OrganizerCore::persistent(const QString &pluginName,
                                   const QString &key,
                                   const QVariant &def) const
{
  return m_Settings.pluginPersistent(pluginName, key, def);
}

void OrganizerCore::setPersistent(const QString &pluginName, const QString &key,
                                  const QVariant &value, bool sync)
{
  m_Settings.setPluginPersistent(pluginName, key, value, sync);
}

QString OrganizerCore::pluginDataPath() const
{
  return qApp->applicationDirPath() + "/" + ToQString(AppConfig::pluginPath())
         + "/data";
}

MOBase::IModInterface *OrganizerCore::installMod(const QString &fileName,
                                                 const QString &initModName)
{
  if (m_CurrentProfile == nullptr) {
    return nullptr;
  }

  if (m_InstallationManager.isRunning()) {
    QMessageBox::information(
      qApp->activeWindow(), tr("Installation cancelled"),
      tr("Another installation is currently in progress."), QMessageBox::Ok);
    return nullptr;
  }

  bool hasIniTweaks = false;
  GuessedValue<QString> modName;
  if (!initModName.isEmpty()) {
    modName.update(initModName, GUESS_USER);
  }
  m_CurrentProfile->writeModlistNow();
  m_InstallationManager.setModsDirectory(m_Settings.getModDirectory());
  if (m_InstallationManager.install(fileName, modName, hasIniTweaks)) {
    MessageDialog::showMessage(tr("Installation successful"),
                               qApp->activeWindow());
    refreshModList();

    int modIndex = ModInfo::getIndex(modName);
    if (modIndex != UINT_MAX) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
      if (hasIniTweaks && (m_UserInterface != nullptr)
          && (QMessageBox::question(qApp->activeWindow(), tr("Configure Mod"),
                                    tr("This mod contains ini tweaks. Do you "
                                       "want to configure them now?"),
                                    QMessageBox::Yes | QMessageBox::No)
              == QMessageBox::Yes)) {
        m_UserInterface->displayModInformation(
          modInfo, modIndex, ModInfoTabIDs::IniFiles);
      }
      m_ModInstalled(modName);
      m_DownloadManager.markInstalled(fileName);
      emit modInstalled(modName);
      return modInfo.data();
    } else {
      reportError(tr("mod not found: %1").arg(qUtf8Printable(modName)));
    }
  } else if (m_InstallationManager.wasCancelled()) {
    QMessageBox::information(qApp->activeWindow(), tr("Installation cancelled"),
                             tr("The mod was not installed completely."),
                             QMessageBox::Ok);
  }
  return nullptr;
}

void OrganizerCore::installDownload(int index)
{
  if (m_InstallationManager.isRunning()) {
    QMessageBox::information(
      qApp->activeWindow(), tr("Installation cancelled"),
      tr("Another installation is currently in progress."), QMessageBox::Ok);
    return;
  }

  try {
    QString fileName = m_DownloadManager.getFilePath(index);
    QString gameName = m_DownloadManager.getGameName(index);
    int modID        = m_DownloadManager.getModID(index);
    int fileID       = m_DownloadManager.getFileInfo(index)->fileID;
    GuessedValue<QString> modName;

    // see if there already are mods with the specified mod id
    if (modID != 0) {
      std::vector<ModInfo::Ptr> modInfo = ModInfo::getByModID(gameName, modID);
      for (auto iter = modInfo.begin(); iter != modInfo.end(); ++iter) {
        std::vector<ModInfo::EFlag> flags = (*iter)->getFlags();
        if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP)
            == flags.end()) {
          modName.update((*iter)->name(), GUESS_PRESET);
          (*iter)->saveMeta();
        }
      }
    }

    m_CurrentProfile->writeModlistNow();

    bool hasIniTweaks = false;
    m_InstallationManager.setModsDirectory(m_Settings.getModDirectory());
    if (m_InstallationManager.install(fileName, modName, hasIniTweaks)) {
      MessageDialog::showMessage(tr("Installation successful"),
                                 qApp->activeWindow());
      refreshModList();

      int modIndex = ModInfo::getIndex(modName);
      if (modIndex != UINT_MAX) {
        ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
        modInfo->addInstalledFile(modID, fileID);

        if (hasIniTweaks && m_UserInterface != nullptr
            && (QMessageBox::question(qApp->activeWindow(), tr("Configure Mod"),
                                      tr("This mod contains ini tweaks. Do you "
                                         "want to configure them now?"),
                                      QMessageBox::Yes | QMessageBox::No)
                == QMessageBox::Yes)) {
          m_UserInterface->displayModInformation(
            modInfo, modIndex, ModInfoTabIDs::IniFiles);
        }

        m_ModInstalled(modName);
      } else {
        reportError(tr("mod not found: %1").arg(qUtf8Printable(modName)));
      }
      m_DownloadManager.markInstalled(index);

      emit modInstalled(modName);
    } else if (m_InstallationManager.wasCancelled()) {
      QMessageBox::information(
          qApp->activeWindow(), tr("Installation cancelled"),
          tr("The mod was not installed completely."), QMessageBox::Ok);
    }
  } catch (const std::exception &e) {
    reportError(e.what());
  }
}

QString OrganizerCore::resolvePath(const QString &fileName) const
{
  if (m_DirectoryStructure == nullptr) {
    return QString();
  }
  const FileEntry::Ptr file
      = m_DirectoryStructure->searchFile(ToWString(fileName), nullptr);
  if (file.get() != nullptr) {
    return ToQString(file->getFullPath());
  } else {
    return QString();
  }
}

QStringList OrganizerCore::listDirectories(const QString &directoryName) const
{
  QStringList result;
  DirectoryEntry *dir = m_DirectoryStructure;
  if (!directoryName.isEmpty())
    dir = dir->findSubDirectoryRecursive(ToWString(directoryName));
  if (dir != nullptr) {
    std::vector<DirectoryEntry *>::iterator current, end;
    dir->getSubDirectories(current, end);
    for (; current != end; ++current) {
      result.append(ToQString((*current)->getName()));
    }
  }
  return result;
}

QStringList OrganizerCore::findFiles(
    const QString &path,
    const std::function<bool(const QString &)> &filter) const
{
  QStringList result;
  DirectoryEntry *dir = m_DirectoryStructure;
  if (!path.isEmpty())
    dir = dir->findSubDirectoryRecursive(ToWString(path));
  if (dir != nullptr) {
    std::vector<FileEntry::Ptr> files = dir->getFiles();
    foreach (FileEntry::Ptr file, files) {
      if (filter(ToQString(file->getFullPath()))) {
        result.append(ToQString(file->getFullPath()));
      }
    }
  }
  return result;
}

QStringList OrganizerCore::getFileOrigins(const QString &fileName) const
{
  QStringList result;
  const FileEntry::Ptr file = m_DirectoryStructure->searchFile(ToWString(fileName), nullptr);

  if (file.get() != nullptr) {
    result.append(ToQString(
        m_DirectoryStructure->getOriginByID(file->getOrigin()).getName()));
    foreach (auto i, file->getAlternatives()) {
      result.append(
          ToQString(m_DirectoryStructure->getOriginByID(i.first).getName()));
    }
  }
  return result;
}

QList<MOBase::IOrganizer::FileInfo> OrganizerCore::findFileInfos(
    const QString &path,
    const std::function<bool(const MOBase::IOrganizer::FileInfo &)> &filter)
    const
{
  QList<IOrganizer::FileInfo> result;
  DirectoryEntry *dir = m_DirectoryStructure;
  if (!path.isEmpty())
    dir = dir->findSubDirectoryRecursive(ToWString(path));
  if (dir != nullptr) {
    std::vector<FileEntry::Ptr> files = dir->getFiles();
    foreach (FileEntry::Ptr file, files) {
      IOrganizer::FileInfo info;
      info.filePath    = ToQString(file->getFullPath());
      bool fromArchive = false;
      info.origins.append(ToQString(
          m_DirectoryStructure->getOriginByID(file->getOrigin(fromArchive))
              .getName()));
      info.archive = fromArchive ? ToQString(file->getArchive().first) : "";
      foreach (auto idx, file->getAlternatives()) {
        info.origins.append(
            ToQString(m_DirectoryStructure->getOriginByID(idx.first).getName()));
      }

      if (filter(info)) {
        result.append(info);
      }
    }
  }
  return result;
}

DownloadManager *OrganizerCore::downloadManager()
{
  return &m_DownloadManager;
}

PluginList *OrganizerCore::pluginList()
{
  return &m_PluginList;
}

ModList *OrganizerCore::modList()
{
  return &m_ModList;
}

QStringList OrganizerCore::modsSortedByProfilePriority() const
{
  QStringList res;
  for (int i = currentProfile()->getPriorityMinimum();
           i < currentProfile()->getPriorityMinimum() + (int)currentProfile()->numRegularMods();
           ++i) {
    int modIndex = currentProfile()->modIndexByPriority(i);
    auto modInfo = ModInfo::getByIndex(modIndex);
    if (!modInfo->hasFlag(ModInfo::FLAG_OVERWRITE) &&
        !modInfo->hasFlag(ModInfo::FLAG_BACKUP)) {
      res.push_back(ModInfo::getByIndex(modIndex)->name());
    }
  }
  return res;
}

QString OrganizerCore::findJavaInstallation(const QString& jarFile)
{
  if (!jarFile.isEmpty()) {
    // try to find java automatically based on the given jar file
    std::wstring jarFileW = jarFile.toStdWString();

    WCHAR buffer[MAX_PATH];
    if (::FindExecutableW(jarFileW.c_str(), nullptr, buffer) > (HINSTANCE)32) {
      DWORD binaryType = 0UL;
      if (!::GetBinaryTypeW(buffer, &binaryType)) {
        qDebug("failed to determine binary type of \"%ls\": %lu", buffer, ::GetLastError());
      } else if (binaryType == SCS_32BIT_BINARY || binaryType == SCS_64BIT_BINARY) {
        return QString::fromWCharArray(buffer);
      }
    }
  }

  // second attempt: look to the registry
  QSettings reg("HKEY_LOCAL_MACHINE\\Software\\JavaSoft\\Java Runtime Environment", QSettings::NativeFormat);
  if (reg.contains("CurrentVersion")) {
    QString currentVersion = reg.value("CurrentVersion").toString();
    return reg.value(QString("%1/JavaHome").arg(currentVersion)).toString().append("\\bin\\javaw.exe");
  }

  // not found
  return {};
}

bool OrganizerCore::getFileExecutionContext(
  QWidget* parent, const QFileInfo &targetInfo,
  QFileInfo &binaryInfo, QString &arguments, FileExecutionTypes& type)
{
  QString extension = targetInfo.suffix();
  if ((extension.compare("cmd", Qt::CaseInsensitive) == 0) ||
    (extension.compare("com", Qt::CaseInsensitive) == 0) ||
    (extension.compare("bat", Qt::CaseInsensitive) == 0)) {
    binaryInfo = QFileInfo("C:\\Windows\\System32\\cmd.exe");
    arguments = QString("/C \"%1\"").arg(QDir::toNativeSeparators(targetInfo.absoluteFilePath()));
    type = FileExecutionTypes::Executable;
    return true;
  } else if (extension.compare("exe", Qt::CaseInsensitive) == 0) {
    binaryInfo = targetInfo;
    type = FileExecutionTypes::Executable;
    return true;
  } else if (extension.compare("jar", Qt::CaseInsensitive) == 0) {
    auto java = findJavaInstallation(targetInfo.absoluteFilePath());

    if (java.isEmpty()) {
      java = QFileDialog::getOpenFileName(
        parent, QObject::tr("Select binary"),
        QString(), QObject::tr("Binary") + " (*.exe)");
    }

    if (java.isEmpty()) {
      return false;
    }

    binaryInfo = QFileInfo(java);
    arguments = QString("-jar \"%1\"").arg(QDir::toNativeSeparators(targetInfo.absoluteFilePath()));
    type = FileExecutionTypes::Executable;

    return true;
  } else {
    type = FileExecutionTypes::Other;
    return true;
  }
}

bool OrganizerCore::executeFileVirtualized(
  QWidget* parent, const QFileInfo& targetInfo)
{
  QFileInfo binaryInfo;
  QString arguments;
  FileExecutionTypes type;

  if (!getFileExecutionContext(parent, targetInfo, binaryInfo, arguments, type)) {
    return false;
  }

  switch (type)
  {
    case FileExecutionTypes::Executable: {
      spawnBinaryDirect(
        binaryInfo, arguments, currentProfile()->name(),
        targetInfo.absolutePath(), "", "");

      return true;
    }

    case FileExecutionTypes::Other: {
      ::ShellExecuteW(nullptr, L"open",
        ToWString(targetInfo.absoluteFilePath()).c_str(),
        nullptr, nullptr, SW_SHOWNORMAL);

      return true;
    }
  }

  // nop
  return false;
}

bool OrganizerCore::previewFileWithAlternatives(
  QWidget* parent, QString fileName, int selectedOrigin)
{
  fileName = QDir::fromNativeSeparators(fileName);

  // what we have is an absolute path to the file in its actual location (for the primary origin)
  // what we want is the path relative to the virtual data directory

  // we need to look in the virtual directory for the file to make sure the info is up to date.

  // check if the file comes from the actual data folder instead of a mod
  QDir gameDirectory = managedGame()->dataDirectory().absolutePath();
  QString relativePath = gameDirectory.relativeFilePath(fileName);
  QDir dirRelativePath = gameDirectory.relativeFilePath(fileName);

  // if the file is on a different drive the dirRelativePath will actually be an
  // absolute path so we make sure that is not the case
  if (!dirRelativePath.isAbsolute() && !relativePath.startsWith("..")) {
    fileName = relativePath;
  }
  else {
    // crude: we search for the next slash after the base mod directory to skip
    // everything up to the data-relative directory
    int offset = settings().getModDirectory().size() + 1;
    offset = fileName.indexOf("/", offset);
    fileName = fileName.mid(offset + 1);
  }



  const FileEntry::Ptr file = directoryStructure()->searchFile(ToWString(fileName), nullptr);

  if (file.get() == nullptr) {
    reportError(tr("file not found: %1").arg(qUtf8Printable(fileName)));
    return false;
  }

  // set up preview dialog
  PreviewDialog preview(fileName);
  auto addFunc = [&](int originId) {
    FilesOrigin &origin = directoryStructure()->getOriginByID(originId);
    QString filePath = QDir::fromNativeSeparators(ToQString(origin.getPath())) + "/" + fileName;
    if (QFile::exists(filePath)) {
      // it's very possible the file doesn't exist, because it's inside an archive. we don't support that
      QWidget *wid = m_PluginContainer->previewGenerator().genPreview(filePath);
      if (wid == nullptr) {
        reportError(tr("failed to generate preview for %1").arg(filePath));
      }
      else {
        preview.addVariant(ToQString(origin.getName()), wid);
      }
    }
  };

  if (selectedOrigin == -1) {
    // don't bother with the vector of origins, just add them as they come
    addFunc(file->getOrigin());
    for (auto alt : file->getAlternatives()) {
      addFunc(alt.first);
    }
  } else {
    std::vector<int> origins;

    // start with the primary origin
    origins.push_back(file->getOrigin());

    // add other origins, push to front if it's the selected one
    for (auto alt : file->getAlternatives()) {
      if (alt.first == selectedOrigin) {
        origins.insert(origins.begin(), alt.first);
      } else {
        origins.push_back(alt.first);
      }
    }

    // can't be empty; either the primary origin was the selected one, or it
    // was one of the alternatives, which got inserted in front

    if (origins[0] != selectedOrigin) {
      // sanity check, this shouldn't happen unless the caller passed an
      // incorrect id

      qWarning().nospace()
        << "selected preview origin " << selectedOrigin << " not found in "
        << "list of alternatives";
    }

    for (int id : origins) {
      addFunc(id);
    }
  }

  if (preview.numVariants() > 0) {
    QSettings &s = settings().directInterface();
    QString key = QString("geometry/%1").arg(preview.objectName());
    if (s.contains(key)) {
      preview.restoreGeometry(s.value(key).toByteArray());
    }

    preview.exec();

    s.setValue(key, preview.saveGeometry());

    return true;
  }
  else {
    QMessageBox::information(
      parent, tr("Sorry"),
      tr("Sorry, can't preview anything. This function currently does not support extracting from bsas."));

    return false;
  }
}

bool OrganizerCore::previewFile(
  QWidget* parent, const QString& originName, const QString& path)
{
  if (!QFile::exists(path)) {
    reportError(tr("File '%1' not found.").arg(path));
    return false;
  }

  PreviewDialog preview(path);

  QWidget *wid = m_PluginContainer->previewGenerator().genPreview(path);
  if (wid == nullptr) {
    reportError(tr("Failed to generate preview for %1").arg(path));
    return false;
  }

  preview.addVariant(originName, wid);

  QSettings &s = settings().directInterface();
  QString key = QString("geometry/%1").arg(preview.objectName());
  if (s.contains(key)) {
    preview.restoreGeometry(s.value(key).toByteArray());
  }

  preview.exec();

  s.setValue(key, preview.saveGeometry());

  return true;
}

void OrganizerCore::spawnBinary(const QFileInfo &binary,
                                const QString &arguments,
                                const QDir &currentDirectory,
                                const QString &steamAppID,
                                const QString &customOverwrite,
                                const QList<MOBase::ExecutableForcedLoadSetting> &forcedLibraries)
{
  DWORD processExitCode = 0;
  HANDLE processHandle = spawnBinaryDirect(binary, arguments, m_CurrentProfile->name(), currentDirectory, steamAppID, customOverwrite, forcedLibraries, &processExitCode);
  if (processHandle != INVALID_HANDLE_VALUE) {
    refreshDirectoryStructure();
    // need to remove our stored load order because it may be outdated if a foreign tool changed the
    // file time. After removing that file, refreshESPList will use the file time as the order
    if (managedGame()->loadOrderMechanism() == IPluginGame::LoadOrderMechanism::FileTime) {
      qDebug("removing loadorder.txt");
      QFile::remove(m_CurrentProfile->getLoadOrderFileName());
    }
    refreshDirectoryStructure();

    refreshESPList(true);
    savePluginList();

    //These callbacks should not fiddle with directoy structure and ESPs.
    m_FinishedRun(binary.absoluteFilePath(), processExitCode);
  }
}

HANDLE OrganizerCore::spawnBinaryDirect(const QFileInfo &binary,
                                        const QString &arguments,
                                        const QString &profileName,
                                        const QDir &currentDirectory,
                                        const QString &steamAppID,
                                        const QString &customOverwrite,
                                        const QList<MOBase::ExecutableForcedLoadSetting> &forcedLibraries,
                                        LPDWORD exitCode)
{
  HANDLE processHandle = spawnBinaryProcess(binary, arguments, profileName, currentDirectory, steamAppID, customOverwrite, forcedLibraries);
  if (Settings::instance().lockGUI() && processHandle != INVALID_HANDLE_VALUE) {
    std::unique_ptr<LockedDialog> dlg;
    ILockedWaitingForProcess* uilock = nullptr;

    if (m_UserInterface != nullptr) {
      uilock = m_UserInterface->lock();
    }
    else {
      // i.e. when running command line shortcuts there is no m_UserInterface
      dlg.reset(new LockedDialog);
      dlg->show();
      dlg->setEnabled(true);
      uilock = dlg.get();
    }

    ON_BLOCK_EXIT([&]() {
      if (m_UserInterface != nullptr) {
        m_UserInterface->unlock();
      } });

    DWORD ignoreExitCode;
    waitForProcessCompletion(processHandle, exitCode ? exitCode : &ignoreExitCode, uilock);
    cycleDiagnostics();
  }

  return processHandle;
}


HANDLE OrganizerCore::spawnBinaryProcess(const QFileInfo &binary,
                                         const QString &arguments,
                                         const QString &profileName,
                                         const QDir &currentDirectory,
                                         const QString &steamAppID,
                                         const QString &customOverwrite,
                                         const QList<MOBase::ExecutableForcedLoadSetting> &forcedLibraries)
{
  prepareStart();

  if (!binary.exists()) {
    reportError(
        tr("Executable not found: %1").arg(qUtf8Printable(binary.absoluteFilePath())));
    return INVALID_HANDLE_VALUE;
  }

  if (!steamAppID.isEmpty()) {
    ::SetEnvironmentVariableW(L"SteamAPPId", ToWString(steamAppID).c_str());
  } else {
    ::SetEnvironmentVariableW(L"SteamAPPId",
                              ToWString(m_Settings.getSteamAppID()).c_str());
  }

  QWidget *window = qApp->activeWindow();
  if ((window != nullptr) && (!window->isVisible())) {
    window = nullptr;
  }

  // This could possibly be extracted somewhere else but it's probably for when
  // we have more than one provider of game registration.
  if ((QFileInfo(
           managedGame()->gameDirectory().absoluteFilePath("steam_api.dll"))
           .exists()
       || QFileInfo(managedGame()->gameDirectory().absoluteFilePath(
                        "steam_api64.dll"))
              .exists())
      && (m_Settings.getLoadMechanism() == LoadMechanism::LOAD_MODORGANIZER)) {

    bool steamFound = true;
    bool steamAccess = true;
    if (!testForSteam(&steamFound, &steamAccess)) {
      qCritical("unable to determine state of Steam");
    }

    if (!steamFound) {
      QDialogButtonBox::StandardButton result;
      result = QuestionBoxMemory::query(window, "steamQuery", binary.fileName(),
                  tr("Start Steam?"),
                  tr("Steam is required to be running already to correctly start the game. "
                    "Should MO try to start steam now?"),
                  QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel);
      if (result == QDialogButtonBox::Yes) {
        startSteam(window);

        // double-check that Steam is started and MO has access
        steamFound = true;
        steamAccess = true;
        if (!testForSteam(&steamFound, &steamAccess)) {
          qCritical("unable to determine state of Steam");
        } else if (!steamFound) {
          qCritical("could not find Steam");
        }

      } else if (result == QDialogButtonBox::Cancel) {
        return INVALID_HANDLE_VALUE;
      }
    }

    if (!steamAccess) {
      QDialogButtonBox::StandardButton result;
      result = QuestionBoxMemory::query(window, "steamAdminQuery", binary.fileName(),
                  tr("Steam: Access Denied"),
                  tr("MO was denied access to the Steam process.  This normally indicates that "
                     "Steam is being run as administrator while MO is not.  This can cause issues "
                     "launching the game.  It is recommended to not run Steam as administrator unless "
                     "absolutely necessary.\n\n"
                     "Restart MO as administrator?"),
                  QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel);
      if (result == QDialogButtonBox::Yes) {
        WCHAR cwd[MAX_PATH];
        if (!GetCurrentDirectory(MAX_PATH, cwd)) {
          qCritical("unable to get current directory (error %d)", GetLastError());
          cwd[0] = L'\0';
        }
        if (!Helper::adminLaunch(
          qApp->applicationDirPath().toStdWString(),
          qApp->applicationFilePath().toStdWString(),
          std::wstring(cwd))) {
          qCritical("unable to relaunch MO as admin");
          return INVALID_HANDLE_VALUE;
        }
        qApp->exit(0);
        return INVALID_HANDLE_VALUE;
      } else if (result == QDialogButtonBox::Cancel) {
        return INVALID_HANDLE_VALUE;
      }
    }
  }

  while (m_DirectoryUpdate) {
    ::Sleep(100);
    QCoreApplication::processEvents();
  }

  // need to make sure all data is saved before we start the application
  if (m_CurrentProfile != nullptr) {
    m_CurrentProfile->writeModlistNow(true);
  }

  // TODO: should also pass arguments
  if (m_AboutToRun(binary.absoluteFilePath())) {
    try {
      m_USVFS.updateMapping(fileMapping(profileName, customOverwrite));
      m_USVFS.updateForcedLibraries(forcedLibraries);

    } catch (const UsvfsConnectorException &e) {
      qDebug(e.what());
      return INVALID_HANDLE_VALUE;
    } catch (const std::exception &e) {
      QMessageBox::warning(window, tr("Error"), e.what());
      return INVALID_HANDLE_VALUE;
    }

    // Check if the Windows Event Logging service is running.  For some reason, this seems to be
    // critical to the successful running of usvfs.
    if (!checkService()) {
      if (QuestionBoxMemory::query(window, QString("eventLogService"), binary.fileName(),
            tr("Windows Event Log Error"),
            tr("The Windows Event Log service is disabled and/or not running.  This prevents"
              " USVFS from running properly.  Your mods may not be working in the executable"
              " that you are launching.  Note that you may have to restart MO and/or your PC"
              " after the service is fixed.\n\nContinue launching %1?").arg(binary.fileName()),
            QDialogButtonBox::Yes | QDialogButtonBox::No) == QDialogButtonBox::No) {
        return INVALID_HANDLE_VALUE;
      }
    }

    for (auto exec : settings().executablesBlacklist().split(";")) {
      if (exec.compare(binary.fileName(), Qt::CaseInsensitive) == 0) {
        if (QuestionBoxMemory::query(window, QString("blacklistedExecutable"), binary.fileName(),
              tr("Blacklisted Executable"),
              tr("The executable you are attempted to launch is blacklisted in the virtual file"
                 " system.  This will likely prevent the executable, and any executables that are"
                 " launched by this one, from seeing any mods.  This could extend to INI files, save"
                 " games and any other virtualized files.\n\nContinue launching %1?").arg(binary.fileName()),
              QDialogButtonBox::Yes | QDialogButtonBox::No) == QDialogButtonBox::No) {
          return INVALID_HANDLE_VALUE;
        }
      }
    }

    QString modsPath = settings().getModDirectory();

    // Check if this a request with either an executable or a working directory under our mods folder
    // then will start the process in a virtualized "environment" with the appropriate paths fixed:
    // (i.e. mods\FNIS\path\exe => game\data\path\exe)
    QString cwdPath = currentDirectory.absolutePath();
    bool virtualizedCwd = cwdPath.startsWith(modsPath, Qt::CaseInsensitive);
    QString binPath = binary.absoluteFilePath();
    bool virtualizedBin = binPath.startsWith(modsPath, Qt::CaseInsensitive);
    if (virtualizedCwd || virtualizedBin) {
      if (virtualizedCwd) {
        int cwdOffset = cwdPath.indexOf('/', modsPath.length() + 1);
        QString adjustedCwd = cwdPath.mid(cwdOffset, -1);
        cwdPath = m_GamePlugin->dataDirectory().absolutePath();
        if (cwdOffset >= 0)
          cwdPath += adjustedCwd;

      }

      if (virtualizedBin) {
        int binOffset = binPath.indexOf('/', modsPath.length() + 1);
        QString adjustedBin = binPath.mid(binOffset, -1);
        binPath = m_GamePlugin->dataDirectory().absolutePath();
        if (binOffset >= 0)
          binPath += adjustedBin;
      }

      QString cmdline
          = QString("launch \"%1\" \"%2\" %3")
                .arg(QDir::toNativeSeparators(cwdPath),
                     QDir::toNativeSeparators(binPath), arguments);

      qDebug() << "Spawning proxyed process <" << cmdline << ">";

      return startBinary(QFileInfo(QCoreApplication::applicationFilePath()),
                         cmdline, QCoreApplication::applicationDirPath(), true);
    } else {
      qDebug() << "Spawning direct process <" << binPath << "," << arguments << "," << cwdPath << ">";
      return startBinary(binary, arguments, currentDirectory, true);
    }
  } else {
    qDebug("start of \"%s\" canceled by plugin",
           qUtf8Printable(binary.absoluteFilePath()));
    return INVALID_HANDLE_VALUE;
  }
}

HANDLE OrganizerCore::runShortcut(const MOShortcut& shortcut)
{
  if (shortcut.hasInstance() && shortcut.instance() != InstanceManager::instance().currentInstance())
    throw std::runtime_error(
      QString("Refusing to run executable from different instance %1:%2")
      .arg(shortcut.instance(),shortcut.executable())
      .toLocal8Bit().constData());

  const Executable& exe = m_ExecutablesList.get(shortcut.executable());

  auto forcedLibaries = m_CurrentProfile->determineForcedLibraries(shortcut.executable());
  if (!m_CurrentProfile->forcedLibrariesEnabled(shortcut.executable())) {
    forcedLibaries.clear();
  }

  return spawnBinaryDirect(
    exe.binaryInfo(), exe.arguments(),
    m_CurrentProfile->name(),
    exe.workingDirectory().length() != 0
    ? exe.workingDirectory()
    : exe.binaryInfo().absolutePath(),
    exe.steamAppID(),
    "",
    forcedLibaries);
}

HANDLE OrganizerCore::startApplication(const QString &executable,
                                       const QStringList &args,
                                       const QString &cwd,
                                       const QString &profile,
                                       const QString &forcedCustomOverwrite,
                                       bool ignoreCustomOverwrite)
{
  QFileInfo binary;
  QString arguments        = args.join(" ");
  QString currentDirectory = cwd;
  QString profileName = profile;
  if (profile.length() == 0) {
    if (m_CurrentProfile != nullptr) {
      profileName = m_CurrentProfile->name();
    } else {
      throw MyException(tr("No profile set"));
    }
  }
  QString steamAppID;
  QString customOverwrite;
  QList<ExecutableForcedLoadSetting> forcedLibraries;
  if (executable.contains('\\') || executable.contains('/')) {
    // file path

    binary = QFileInfo(executable);
    if (binary.isRelative()) {
      // relative path, should be relative to game directory
      binary = QFileInfo(
          managedGame()->gameDirectory().absoluteFilePath(executable));
    }
    if (cwd.length() == 0) {
      currentDirectory = binary.absolutePath();
    }
    try {
      const Executable &exe = m_ExecutablesList.getByBinary(binary);
      steamAppID = exe.steamAppID();
      customOverwrite
          = m_CurrentProfile->setting("custom_overwrites", exe.title())
                .toString();
      if (m_CurrentProfile->forcedLibrariesEnabled(exe.title())) {
        forcedLibraries = m_CurrentProfile->determineForcedLibraries(exe.title());
      }
    } catch (const std::runtime_error &) {
      // nop
    }
  } else {
    // only a file name, search executables list
    try {
      const Executable &exe = m_ExecutablesList.get(executable);
      steamAppID = exe.steamAppID();
      customOverwrite
          = m_CurrentProfile->setting("custom_overwrites", exe.title())
                .toString();
      if (m_CurrentProfile->forcedLibrariesEnabled(exe.title())) {
        forcedLibraries = m_CurrentProfile->determineForcedLibraries(exe.title());
      }
      if (arguments == "") {
        arguments = exe.arguments();
      }
      binary = exe.binaryInfo();
      if (cwd.length() == 0) {
        currentDirectory = exe.workingDirectory();
      }
    } catch (const std::runtime_error &) {
      qWarning("\"%s\" not set up as executable",
               qUtf8Printable(executable));
      binary = QFileInfo(executable);
    }
  }

  if (!forcedCustomOverwrite.isEmpty())
    customOverwrite = forcedCustomOverwrite;
  if (ignoreCustomOverwrite)
    customOverwrite.clear();

  return spawnBinaryDirect(binary,
                           arguments,
                           profileName,
                           currentDirectory,
                           steamAppID,
                           customOverwrite,
                           forcedLibraries);
}

bool OrganizerCore::waitForApplication(HANDLE handle, LPDWORD exitCode)
{
  if (!Settings::instance().lockGUI())
    return true;

  ILockedWaitingForProcess* uilock = nullptr;
  if (m_UserInterface != nullptr) {
    uilock = m_UserInterface->lock();
  }

  ON_BLOCK_EXIT([&] () {
    if (m_UserInterface != nullptr) {
      m_UserInterface->unlock();
    } });
  return waitForProcessCompletion(handle, exitCode, uilock);
}

bool OrganizerCore::waitForProcessCompletion(HANDLE handle, LPDWORD exitCode, ILockedWaitingForProcess* uilock)
{
  bool originalHandle = true;
  bool newHandle = true;
  bool uiunlocked = false;

  DWORD currentPID = 0;
  QString processName;
  auto waitForChildUntil = GetTickCount64();
  if (handle != INVALID_HANDLE_VALUE) {
    currentPID = GetProcessId(handle);
    processName = QString::fromStdWString(getProcessName(handle));
  }

  // Certain process names we wish to "hide" for aesthetic reason:
  bool waitingOnHidden = false;
  std::vector<QString> hiddenList;
  hiddenList.push_back(QFileInfo(QCoreApplication::applicationFilePath()).fileName());
  for (QString hide : hiddenList)
    if (processName.contains(hide, Qt::CaseInsensitive))
      waitingOnHidden = true;
  // The main reason for adding the hidden list is to hide the MO proxy we use to spawn virtualized processes.
  // On the one hand we want to display the real executable without it feeling laggy, on the other we don't want
  // to requery processes all the time if for some reason we are waiting on hidden processes and find no "unhidden"
  // process. For this reason we use exponential backoff and also start with a delibrately low value to improve
  // the responsiveness of the initial update
  DWORD64 nextHiddenCheck = GetTickCount64();
  DWORD64 nextHiddenCheckDelay = 50;

  constexpr DWORD INPUT_EVENT = WAIT_OBJECT_0 + 1;
  DWORD res = WAIT_TIMEOUT;
  while (handle != INVALID_HANDLE_VALUE && (newHandle || res == WAIT_TIMEOUT || res == INPUT_EVENT))
  {
    if (newHandle) {
      processName += QString(" (%1)").arg(currentPID);
      if (uilock)
        uilock->setProcessName(processName);
      qDebug() << "Waiting for"
        << (originalHandle ? "spawned" : "usvfs")
        << "process completion :" << qUtf8Printable(processName);
      newHandle = false;
    }

    // Wait for a an event on the handle, a key press, mouse click or timeout
    res = MsgWaitForMultipleObjects(1, &handle, FALSE, 200, QS_KEY | QS_MOUSEBUTTON);
    if (res == WAIT_FAILED) {
      qWarning() << "Failed waiting for process completion : MsgWaitForMultipleObjects WAIT_FAILED" << GetLastError();
      break;
    }

    // keep processing events so the app doesn't appear dead
    QCoreApplication::sendPostedEvents();
    QCoreApplication::processEvents();

    if (uilock && uilock->unlockForced()) {
      uiunlocked = true;
      break;
    }

    if (res == WAIT_OBJECT_0) {
      // process we were waiting on has completed
      if (originalHandle && exitCode && !::GetExitCodeProcess(handle, exitCode))
        qWarning() << "Failed getting exit code of complete process :" << GetLastError();
      CloseHandle(handle);
      handle = INVALID_HANDLE_VALUE;
      originalHandle = false;
      // if the previous process spawned a child process and immediately exits we may miss it if we check immediately
      waitForChildUntil = GetTickCount64() + 800;
    }

    // search for another process to wait on if either:
    // 1. we just completed waiting for a process and need to find/wait for an inject child
    // 2. we are currently waiting on a hidden process so periodically check if there is a non-hidden process to wait on
    bool firstIteration = true;
    while ((handle == INVALID_HANDLE_VALUE && GetTickCount64() <= waitForChildUntil)
            || (waitingOnHidden && GetTickCount64() >= nextHiddenCheck))
    {
      if (firstIteration)
        firstIteration = false;
      else {
        QThread::msleep(200);
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
      }

      // search if there is another usvfs process active
      handle = findAndOpenAUSVFSProcess(hiddenList, currentPID);
      waitingOnHidden = false;
      newHandle = handle != INVALID_HANDLE_VALUE;
      if (newHandle) {
        currentPID = GetProcessId(handle);
        processName = QString::fromStdWString(getProcessName(handle));
        for (QString hide : hiddenList)
          if (processName.contains(hide, Qt::CaseInsensitive))
            waitingOnHidden = true;
      }
      if (waitingOnHidden) {
        nextHiddenCheck = GetTickCount64() + nextHiddenCheckDelay;
        nextHiddenCheckDelay = std::min(nextHiddenCheckDelay * 2, (DWORD64) 2000);
      }
      else {
        nextHiddenCheck = GetTickCount64();
        nextHiddenCheckDelay = 200;
      }
    }
  }

  if (res == WAIT_OBJECT_0)
    qDebug() << "Waiting for process completion successfull";
  else if (uiunlocked)
    qDebug() << "Waiting for process completion aborted by UI";
  else
    qDebug() << "Waiting for process completion not successfull :" << res;

  if (handle != INVALID_HANDLE_VALUE)
    ::CloseHandle(handle);

  return res == WAIT_OBJECT_0;
}

HANDLE OrganizerCore::findAndOpenAUSVFSProcess(const std::vector<QString>& hiddenList, DWORD preferedParentPid) {
  // for practical reasons a querySize of 1 is probably enough, we use a larger query as a heuristics
  // to find a more "aesthetic injected processes (attempting to comply to hiddenList and preferedParentPid)
  constexpr size_t querySize = 100;
  DWORD pids[querySize];
  size_t found = querySize;
  if (!::GetVFSProcessList(&found, pids)) {
    qWarning() << "Failed seeking USVFS processes : GetVFSProcessList failed?!";
    return INVALID_HANDLE_VALUE;
  }

  HANDLE best_match = INVALID_HANDLE_VALUE;
  bool best_match_hidden = true;
  for (size_t i = 0; i < found; ++i) {
    if (pids[i] == GetCurrentProcessId())
      continue; // obviously don't wait for MO process

    HANDLE handle = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pids[i]);
    if (handle == INVALID_HANDLE_VALUE) {
      qWarning() << "Failed openning USVFS process " << pids[i] << " : OpenProcess failed" << GetLastError();
      continue;
    }

    QString pname = QString::fromStdWString(getProcessName(handle));
    bool phidden = false;
    for (auto hide : hiddenList)
      if (pname.contains(hide, Qt::CaseInsensitive))
        phidden = true;

    bool pprefered = preferedParentPid && getProcessParentID(pids[i]) == preferedParentPid;

    if (best_match == INVALID_HANDLE_VALUE || best_match_hidden || (!phidden && pprefered)) {
      if (best_match != INVALID_HANDLE_VALUE)
        CloseHandle(best_match);
      best_match = handle;
      best_match_hidden = phidden;
    }
    else
      CloseHandle(handle);

    if (!phidden && pprefered)
      return best_match;
  }

  return best_match;
}

bool OrganizerCore::onAboutToRun(
    const std::function<bool(const QString &)> &func)
{
  auto conn = m_AboutToRun.connect(func);
  return conn.connected();
}

bool OrganizerCore::onFinishedRun(
    const std::function<void(const QString &, unsigned int)> &func)
{
  auto conn = m_FinishedRun.connect(func);
  return conn.connected();
}

bool OrganizerCore::onModInstalled(
    const std::function<void(const QString &)> &func)
{
  auto conn = m_ModInstalled.connect(func);
  return conn.connected();
}

void OrganizerCore::refreshModList(bool saveChanges)
{
  // don't lose changes!
  if (saveChanges) {
    m_CurrentProfile->writeModlistNow(true);
  }
  ModInfo::updateFromDisc(m_Settings.getModDirectory(), &m_DirectoryStructure,
                          m_PluginContainer, m_Settings.displayForeign(), managedGame());

  m_CurrentProfile->refreshModStatus();

  m_ModList.notifyChange(-1);

  refreshDirectoryStructure();
}

void OrganizerCore::refreshESPList(bool force)
{
  if (m_DirectoryUpdate) {
    // don't mess up the esp list if we're currently updating the directory
    // structure
    m_PostRefreshTasks.append([=]() {
      this->refreshESPList(force);
    });
    return;
  }
  m_CurrentProfile->writeModlist();

  // clear list
  try {
    m_PluginList.refresh(m_CurrentProfile->name(), *m_DirectoryStructure,
                         m_CurrentProfile->getLockedOrderFileName(), force);
  } catch (const std::exception &e) {
    reportError(tr("Failed to refresh list of esps: %1").arg(e.what()));
  }
}

void OrganizerCore::refreshBSAList()
{
  DataArchives *archives = m_GamePlugin->feature<DataArchives>();

  if (archives != nullptr) {
    m_ArchivesInit = false;

    // default archives are the ones enabled outside MO. if the list can't be
    // found (which might
    // happen if ini files are missing) use hard-coded defaults (preferrably the
    // same the game would use)
    m_DefaultArchives = archives->archives(m_CurrentProfile);
    if (m_DefaultArchives.length() == 0) {
      m_DefaultArchives = archives->vanillaArchives();
    }

    m_ActiveArchives.clear();

    auto iter        = enabledArchives();
    m_ActiveArchives = toStringList(iter.begin(), iter.end());
    if (m_ActiveArchives.isEmpty()) {
      m_ActiveArchives = m_DefaultArchives;
    }

    if (m_UserInterface != nullptr) {
      m_UserInterface->updateBSAList(m_DefaultArchives, m_ActiveArchives);
    }

    m_ArchivesInit = true;
  }
}

void OrganizerCore::refreshLists()
{
  if ((m_CurrentProfile != nullptr) && m_DirectoryStructure->isPopulated()) {
    refreshESPList(true);
    refreshBSAList();
  } // no point in refreshing lists if no files have been added to the directory
    // tree
}

void OrganizerCore::updateModActiveState(int index, bool active)
{
  QList<unsigned int> modsToUpdate;
  modsToUpdate.append(index);
  updateModsActiveState(modsToUpdate, active);
}

void OrganizerCore::updateModsActiveState(const QList<unsigned int> &modIndices, bool active)
{
  int enabled = 0;
  for (auto index : modIndices) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
    QDir dir(modInfo->absolutePath());
    for (const QString &esm :
      dir.entryList(QStringList() << "*.esm", QDir::Files)) {
      const FileEntry::Ptr file = m_DirectoryStructure->findFile(ToWString(esm));
      if (file.get() == nullptr) {
        qWarning("failed to activate %s", qUtf8Printable(esm));
        continue;
      }

      if (active != m_PluginList.isEnabled(esm)
        && file->getAlternatives().empty()) {
        m_PluginList.blockSignals(true);
        m_PluginList.enableESP(esm, active);
        m_PluginList.blockSignals(false);
      }
    }

    for (const QString &esl :
      dir.entryList(QStringList() << "*.esl", QDir::Files)) {
      const FileEntry::Ptr file = m_DirectoryStructure->findFile(ToWString(esl));
      if (file.get() == nullptr) {
        qWarning("failed to activate %s", qUtf8Printable(esl));
        continue;
      }

      if (active != m_PluginList.isEnabled(esl)
        && file->getAlternatives().empty()) {
        m_PluginList.blockSignals(true);
        m_PluginList.enableESP(esl, active);
        m_PluginList.blockSignals(false);
        ++enabled;
      }
    }
    QStringList esps = dir.entryList(QStringList() << "*.esp", QDir::Files);
    for (const QString &esp : esps) {
      const FileEntry::Ptr file = m_DirectoryStructure->findFile(ToWString(esp));
      if (file.get() == nullptr) {
        qWarning("failed to activate %s", qUtf8Printable(esp));
        continue;
      }

      if (active != m_PluginList.isEnabled(esp)
        && file->getAlternatives().empty()) {
        m_PluginList.blockSignals(true);
        m_PluginList.enableESP(esp, active);
        m_PluginList.blockSignals(false);
        ++enabled;
      }
    }
  }
  if (active && (enabled > 1)) {
    MessageDialog::showMessage(
      tr("Multiple esps/esls activated, please check that they don't conflict."),
      qApp->activeWindow());
  }
  m_PluginList.refreshLoadOrder();
  // immediately save affected lists
  m_PluginListsWriter.writeImmediately(false);
}

void OrganizerCore::updateModInDirectoryStructure(unsigned int index,
                                                  ModInfo::Ptr modInfo)
{
  QMap<unsigned int, ModInfo::Ptr> allModInfo;
  allModInfo[index] = modInfo;
  updateModsInDirectoryStructure(allModInfo);
}

void OrganizerCore::updateModsInDirectoryStructure(QMap<unsigned int, ModInfo::Ptr> modInfo)
{
  for (auto idx : modInfo.keys()) {
    // add files of the bsa to the directory structure
    m_DirectoryRefresher.addModFilesToStructure(
      m_DirectoryStructure, modInfo[idx]->name(),
      m_CurrentProfile->getModPriority(idx), modInfo[idx]->absolutePath(),
      modInfo[idx]->stealFiles());
  }
  DirectoryRefresher::cleanStructure(m_DirectoryStructure);
  // need to refresh plugin list now so we can activate esps
  refreshESPList(true);
  // activate all esps of the specified mod so the bsas get activated along with
  // it
  m_PluginList.blockSignals(true);
  updateModsActiveState(modInfo.keys(), true);
  m_PluginList.blockSignals(false);
  // now we need to refresh the bsa list and save it so there is no confusion
  // about what archives are avaiable and active
  refreshBSAList();
  if (m_UserInterface != nullptr) {
    m_UserInterface->archivesWriter().writeImmediately(false);
  }

  std::vector<QString> archives = enabledArchives();
  m_DirectoryRefresher.setMods(
    m_CurrentProfile->getActiveMods(),
    std::set<QString>(archives.begin(), archives.end()));

  // finally also add files from bsas to the directory structure
  for (auto idx : modInfo.keys()) {
    m_DirectoryRefresher.addModBSAToStructure(
      m_DirectoryStructure, modInfo[idx]->name(),
      m_CurrentProfile->getModPriority(idx), modInfo[idx]->absolutePath(),
      modInfo[idx]->archives());
  }
}

void OrganizerCore::loggedInAction(QWidget* parent, std::function<void ()> f)
{
  if (NexusInterface::instance(m_PluginContainer)->getAccessManager()->validated()) {
    f();
  } else {
    QString apiKey;
    if (settings().getNexusApiKey(apiKey)) {
      doAfterLogin([f]{ f(); });
      NexusInterface::instance(m_PluginContainer)->getAccessManager()->apiCheck(apiKey);
    } else {
      MessageDialog::showMessage(tr("You need to be logged in with Nexus"), parent);
    }
  }
}

void OrganizerCore::requestDownload(const QUrl &url, QNetworkReply *reply)
{
  if (m_PluginContainer != nullptr) {
    for (IPluginModPage *modPage :
         m_PluginContainer->plugins<MOBase::IPluginModPage>()) {
      ModRepositoryFileInfo *fileInfo = new ModRepositoryFileInfo();
      if (modPage->handlesDownload(url, reply->url(), *fileInfo)) {
        fileInfo->repository = modPage->name();
        m_DownloadManager.addDownload(reply, fileInfo);
        return;
      }
    }
  }

  // no mod found that could handle the download. Is it a nexus mod?
  if (url.host() == "www.nexusmods.com") {
    QString gameName = "";
    int modID  = 0;
    int fileID = 0;
    QRegExp nameExp("www\\.nexusmods\\.com/(\\a+)/");
    if (nameExp.indexIn(url.toString()) != -1) {
      gameName = nameExp.cap(1);
    }
    QRegExp modExp("mods/(\\d+)");
    if (modExp.indexIn(url.toString()) != -1) {
      modID = modExp.cap(1).toInt();
    }
    QRegExp fileExp("fid=(\\d+)");
    if (fileExp.indexIn(reply->url().toString()) != -1) {
      fileID = fileExp.cap(1).toInt();
    }
    m_DownloadManager.addDownload(reply,
                                  new ModRepositoryFileInfo(gameName, modID, fileID));
  } else {
    if (QMessageBox::question(qApp->activeWindow(), tr("Download?"),
                              tr("A download has been started but no installed "
                                 "page plugin recognizes it.\n"
                                 "If you download anyway no information (i.e. "
                                 "version) will be associated with the "
                                 "download.\n"
                                 "Continue?"),
                              QMessageBox::Yes | QMessageBox::No)
        == QMessageBox::Yes) {
      m_DownloadManager.addDownload(reply, new ModRepositoryFileInfo());
    }
  }
}

ModListSortProxy *OrganizerCore::createModListProxyModel()
{
  ModListSortProxy *result = new ModListSortProxy(m_CurrentProfile, this);
  result->setSourceModel(&m_ModList);
  return result;
}

PluginListSortProxy *OrganizerCore::createPluginListProxyModel()
{
  PluginListSortProxy *result = new PluginListSortProxy(this);
  result->setSourceModel(&m_PluginList);
  return result;
}

IPluginGame const *OrganizerCore::managedGame() const
{
  return m_GamePlugin;
}

std::vector<QString> OrganizerCore::enabledArchives()
{
  std::vector<QString> result;
  if (m_ArchiveParsing) {
    QFile archiveFile(m_CurrentProfile->getArchivesFileName());
    if (archiveFile.open(QIODevice::ReadOnly)) {
      while (!archiveFile.atEnd()) {
        result.push_back(QString::fromUtf8(archiveFile.readLine()).trimmed());
      }
      archiveFile.close();
    }
  }
  return result;
}

void OrganizerCore::refreshDirectoryStructure()
{
  if (!m_DirectoryUpdate) {
    m_CurrentProfile->writeModlistNow(true);

    m_DirectoryUpdate = true;
    std::vector<std::tuple<QString, QString, int>> activeModList
        = m_CurrentProfile->getActiveMods();
    auto archives = enabledArchives();
    m_DirectoryRefresher.setMods(
        activeModList, std::set<QString>(archives.begin(), archives.end()));

    QTimer::singleShot(0, &m_DirectoryRefresher, SLOT(refresh()));
  }
}

void OrganizerCore::directory_refreshed()
{
  DirectoryEntry *newStructure = m_DirectoryRefresher.getDirectoryStructure();
  Q_ASSERT(newStructure != m_DirectoryStructure);
  if (newStructure != nullptr) {
    std::swap(m_DirectoryStructure, newStructure);
    delete newStructure;
  } else {
    // TODO: don't know why this happens, this slot seems to get called twice
    // with only one emit
    return;
  }
  m_DirectoryUpdate = false;

  for (int i = 0; i < m_ModList.rowCount(); ++i) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(i);
    modInfo->clearCaches();
  }
  for (auto task : m_PostRefreshTasks) {
    task();
  }
  m_PostRefreshTasks.clear();

  if (m_CurrentProfile != nullptr) {
    refreshLists();
  }
}

void OrganizerCore::profileRefresh()
{
  // have to refresh mods twice (again in refreshModList), otherwise the refresh
  // isn't complete. Not sure why
  ModInfo::updateFromDisc(m_Settings.getModDirectory(), &m_DirectoryStructure,
                          m_PluginContainer, m_Settings.displayForeign(), managedGame());
  m_CurrentProfile->refreshModStatus();

  refreshModList();
}

void OrganizerCore::modStatusChanged(unsigned int index)
{
  try {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
    if (m_CurrentProfile->modEnabled(index)) {
      updateModInDirectoryStructure(index, modInfo);
    } else {
      updateModActiveState(index, false);
      if (m_DirectoryStructure->originExists(ToWString(modInfo->name()))) {
        FilesOrigin &origin
            = m_DirectoryStructure->getOriginByName(ToWString(modInfo->name()));
        origin.enable(false);
      }
      if (m_UserInterface != nullptr) {
        m_UserInterface->archivesWriter().write();
      }
    }
    modInfo->clearCaches();

    for (unsigned int i = 0; i < m_CurrentProfile->numMods(); ++i) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(i);
      int priority = m_CurrentProfile->getModPriority(i);
      if (m_DirectoryStructure->originExists(ToWString(modInfo->name()))) {
        // priorities in the directory structure are one higher because data is
        // 0
        m_DirectoryStructure->getOriginByName(ToWString(modInfo->name()))
            .setPriority(priority + 1);
      }
    }
    m_DirectoryStructure->getFileRegister()->sortOrigins();

    refreshLists();
  } catch (const std::exception &e) {
    reportError(tr("failed to update mod list: %1").arg(e.what()));
  }
}

void OrganizerCore::modStatusChanged(QList<unsigned int> index) {
  try {
    QMap<unsigned int, ModInfo::Ptr> modsToEnable;
    QMap<unsigned int, ModInfo::Ptr> modsToDisable;
    for (auto idx : index) {
      if (m_CurrentProfile->modEnabled(idx)) {
        modsToEnable[idx] = ModInfo::getByIndex(idx);
      } else {
        modsToDisable[idx] = ModInfo::getByIndex(idx);
      }
    }
    if (!modsToEnable.isEmpty()) {
      updateModsInDirectoryStructure(modsToEnable);
      for (auto modInfo : modsToEnable.values()) {
        modInfo->clearCaches();
      }
    }
    if (!modsToDisable.isEmpty()) {
      updateModsActiveState(modsToDisable.keys(), false);
      for (auto idx : modsToDisable.keys()) {
        if (m_DirectoryStructure->originExists(ToWString(modsToDisable[idx]->name()))) {
          FilesOrigin &origin
            = m_DirectoryStructure->getOriginByName(ToWString(modsToDisable[idx]->name()));
          origin.enable(false);
        }
      }
      if (m_UserInterface != nullptr) {
        m_UserInterface->archivesWriter().write();
      }
    }

    for (unsigned int i = 0; i < m_CurrentProfile->numMods(); ++i) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(i);
      int priority = m_CurrentProfile->getModPriority(i);
      if (m_DirectoryStructure->originExists(ToWString(modInfo->name()))) {
        // priorities in the directory structure are one higher because data is
        // 0
        m_DirectoryStructure->getOriginByName(ToWString(modInfo->name()))
          .setPriority(priority + 1);
      }
    }
    m_DirectoryStructure->getFileRegister()->sortOrigins();

    refreshLists();
  } catch (const std::exception &e) {
    reportError(tr("failed to update mod list: %1").arg(e.what()));
  }
}

void OrganizerCore::loginSuccessful(bool necessary)
{
  if (necessary) {
    MessageDialog::showMessage(tr("login successful"), qApp->activeWindow());
  }
  for (QString url : m_PendingDownloads) {
    downloadRequestedNXM(url);
  }
  m_PendingDownloads.clear();
  for (auto task : m_PostLoginTasks) {
    task();
  }

  m_PostLoginTasks.clear();
  NexusInterface::instance(m_PluginContainer)->loginCompleted();
}

void OrganizerCore::loginSuccessfulUpdate(bool necessary)
{
  if (necessary) {
    MessageDialog::showMessage(tr("login successful"), qApp->activeWindow());
  }
  m_Updater.startUpdate();
}

void OrganizerCore::loginFailed(const QString &message)
{
  qDebug().nospace().noquote()
    << "Nexus API validation failed: " << message;

  if (QMessageBox::question(qApp->activeWindow(), tr("Login failed"),
                            tr("Login failed, try again?"))
      == QMessageBox::Yes) {
    if (nexusApi(true)) {
      return;
    }
  }

  if (!m_PendingDownloads.isEmpty()) {
    MessageDialog::showMessage(
        tr("login failed: %1. Download will not be associated with an account")
            .arg(message),
        qApp->activeWindow());
    for (QString url : m_PendingDownloads) {
      downloadRequestedNXM(url);
    }
    m_PendingDownloads.clear();
  } else {
    MessageDialog::showMessage(tr("login failed: %1").arg(message),
                               qApp->activeWindow());
    m_PostLoginTasks.clear();
  }
  NexusInterface::instance(m_PluginContainer)->loginCompleted();
}

void OrganizerCore::loginFailedUpdate(const QString &message)
{
  MessageDialog::showMessage(
      tr("login failed: %1. You need to log-in with Nexus to update MO.")
          .arg(message),
      qApp->activeWindow());
}

void OrganizerCore::syncOverwrite()
{
  unsigned int overwriteIndex         = ModInfo::findMod([](ModInfo::Ptr mod) -> bool {
    std::vector<ModInfo::EFlag> flags = mod->getFlags();
    return std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE)
           != flags.end();
  });

  ModInfo::Ptr modInfo = ModInfo::getByIndex(overwriteIndex);
  SyncOverwriteDialog syncDialog(modInfo->absolutePath(), m_DirectoryStructure,
                                 qApp->activeWindow());
  if (syncDialog.exec() == QDialog::Accepted) {
    syncDialog.apply(QDir::fromNativeSeparators(m_Settings.getModDirectory()));
    modInfo->testValid();
    refreshDirectoryStructure();
  }
}

QString OrganizerCore::oldMO1HookDll() const
{
  if (auto extender = managedGame()->feature<ScriptExtender>()) {
    QString hookdll = QDir::toNativeSeparators(
      managedGame()->dataDirectory().absoluteFilePath(extender->PluginPath() + "/hook.dll"));
    if (QFile(hookdll).exists())
      return hookdll;
  }
  return QString();
}

std::vector<unsigned int> OrganizerCore::activeProblems() const
{
  std::vector<unsigned int> problems;
  const auto& hookdll = oldMO1HookDll();
  if (!hookdll.isEmpty()) {
    // This warning will now be shown every time the problems are checked, which is a bit
    // of a "log spam". But since this is a sevre error which will most likely make the
    // game crash/freeze/etc. and is very hard to diagnose,  this "log spam" will make it
    // easier for the user to notice the warning.
    qWarning("hook.dll found in game folder: %s", qUtf8Printable(hookdll));
    problems.push_back(PROBLEM_MO1SCRIPTEXTENDERWORKAROUND);
  }
  return problems;
}

QString OrganizerCore::shortDescription(unsigned int key) const
{
  switch (key) {
    case PROBLEM_MO1SCRIPTEXTENDERWORKAROUND: {
      return tr("MO1 \"Script Extender\" load mechanism has left hook.dll in your game folder");
    } break;
    default: {
      return tr("Description missing");
    } break;
  }
}

QString OrganizerCore::fullDescription(unsigned int key) const
{
  switch (key) {
    case PROBLEM_MO1SCRIPTEXTENDERWORKAROUND: {
      return tr("<a href=\"%1\">hook.dll</a> has been found in your game folder (right click to copy the full path). "
                "This is most likely a leftover of setting the ModOrganizer 1 load mechanism to \"Script Extender\", "
                "in which case you must remove this file either by changing the load mechanism in ModOrganizer 1 or "
                "manually removing the file, otherwise the game is likely to crash and burn.").arg(oldMO1HookDll());
      break;
    }
    default: {
      return tr("Description missing");
    } break;
  }
}

bool OrganizerCore::hasGuidedFix(unsigned int) const
{
  return false;
}

void OrganizerCore::startGuidedFix(unsigned int) const
{
}

bool OrganizerCore::saveCurrentLists()
{
  if (m_DirectoryUpdate) {
    qWarning("not saving lists during directory update");
    return false;
  }

  try {
    savePluginList();
    if (m_UserInterface != nullptr) {
      m_UserInterface->archivesWriter().write();
    }
  } catch (const std::exception &e) {
    reportError(tr("failed to save load order: %1").arg(e.what()));
  }

  return true;
}

void OrganizerCore::savePluginList()
{
  if (m_DirectoryUpdate) {
    // delay save till after directory update
    m_PostRefreshTasks.append([this]() {
      this->savePluginList();
    });
    return;
  }
  m_PluginList.saveTo(m_CurrentProfile->getLockedOrderFileName(),
                      m_CurrentProfile->getDeleterFileName(),
                      m_Settings.hideUncheckedPlugins());
  m_PluginList.saveLoadOrder(*m_DirectoryStructure);
}

void OrganizerCore::prepareStart()
{
  if (m_CurrentProfile == nullptr) {
    return;
  }
  m_CurrentProfile->writeModlist();
  m_CurrentProfile->createTweakedIniFile();
  saveCurrentLists();
  m_Settings.setupLoadMechanism();
  storeSettings();
}

std::vector<Mapping> OrganizerCore::fileMapping(const QString &profileName,
                                                const QString &customOverwrite)
{
  // need to wait until directory structure
  while (m_DirectoryUpdate) {
    ::Sleep(100);
    QCoreApplication::processEvents();
  }

  IPluginGame *game  = qApp->property("managed_game").value<IPluginGame *>();
  Profile profile(QDir(m_Settings.getProfileDirectory() + "/" + profileName),
                  game);

  MappingType result;

  QString dataPath
      = QDir::toNativeSeparators(game->dataDirectory().absolutePath());

  bool overwriteActive = false;

  for (auto mod : profile.getActiveMods()) {
    if (std::get<0>(mod).compare("overwrite", Qt::CaseInsensitive) == 0) {
      continue;
    }

    unsigned int modIndex = ModInfo::getIndex(std::get<0>(mod));
    ModInfo::Ptr modPtr   = ModInfo::getByIndex(modIndex);

    bool createTarget = customOverwrite == std::get<0>(mod);

    overwriteActive |= createTarget;

    if (modPtr->isRegular()) {
      result.insert(result.end(), {QDir::toNativeSeparators(std::get<1>(mod)),
                                   dataPath, true, createTarget});
    }
  }

  if (!overwriteActive && !customOverwrite.isEmpty()) {
    throw MyException(tr("The designated write target \"%1\" is not enabled.")
                          .arg(customOverwrite));
  }

  if (m_CurrentProfile->localSavesEnabled()) {
    LocalSavegames *localSaves = game->feature<LocalSavegames>();
    if (localSaves != nullptr) {
      MappingType saveMap
          = localSaves->mappings(currentProfile()->absolutePath() + "/saves");
      result.reserve(result.size() + saveMap.size());
      result.insert(result.end(), saveMap.begin(), saveMap.end());
    } else {
      qWarning("local save games not supported by this game plugin");
    }
  }

  result.insert(result.end(), {
                  QDir::toNativeSeparators(m_Settings.getOverwriteDirectory()),
                  dataPath,
                  true,
                  customOverwrite.isEmpty()
                });

  for (MOBase::IPluginFileMapper *mapper :
       m_PluginContainer->plugins<MOBase::IPluginFileMapper>()) {
    IPlugin *plugin = dynamic_cast<IPlugin *>(mapper);
    if (plugin->isActive()) {
      MappingType pluginMap = mapper->mappings();
      result.reserve(result.size() + pluginMap.size());
      result.insert(result.end(), pluginMap.begin(), pluginMap.end());
    }
  }

  return result;
}


std::vector<Mapping> OrganizerCore::fileMapping(
    const QString &dataPath, const QString &relPath, const DirectoryEntry *base,
    const DirectoryEntry *directoryEntry, int createDestination)
{
  std::vector<Mapping> result;

  for (FileEntry::Ptr current : directoryEntry->getFiles()) {
    bool isArchive = false;
    int origin = current->getOrigin(isArchive);
    if (isArchive || (origin == 0)) {
      continue;
    }

    QString originPath
        = QString::fromStdWString(base->getOriginByID(origin).getPath());
    QString fileName = QString::fromStdWString(current->getName());
//    QString fileName = ToQString(current->getName());
    QString source   = originPath + relPath + fileName;
    QString target   = dataPath + relPath + fileName;
    if (source != target) {
      result.push_back({source, target, false, false});
    }
  }

  // recurse into subdirectories
  std::vector<DirectoryEntry *>::const_iterator current, end;
  directoryEntry->getSubDirectories(current, end);
  for (; current != end; ++current) {
    int origin = (*current)->anyOrigin();

    QString originPath
        = QString::fromStdWString(base->getOriginByID(origin).getPath());
    QString dirName = QString::fromStdWString((*current)->getName());
    QString source  = originPath + relPath + dirName;
    QString target  = dataPath + relPath + dirName;

    bool writeDestination
        = (base == directoryEntry) && (origin == createDestination);

    result.push_back({source, target, true, writeDestination});
    std::vector<Mapping> subRes = fileMapping(
        dataPath, relPath + dirName + "\\", base, *current, createDestination);
    result.insert(result.end(), subRes.begin(), subRes.end());
  }
  return result;
}
