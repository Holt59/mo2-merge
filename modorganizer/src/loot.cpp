#include "loot.h"
#include "spawn.h"
#include "organizercore.h"
#include <log.h>
#include <report.h>

using namespace MOBase;


class LootDialog : public QDialog
{
public:
  LootDialog(QWidget* parent, OrganizerCore& core, Loot& loot) :
    QDialog(parent), m_core(core), m_loot(loot),
    m_label(nullptr), m_progress(nullptr), m_buttons(nullptr), m_finished(false)
  {
    createUI();

    QObject::connect(
      &m_loot, &Loot::output, this,
      [&](auto&& s){ addOutput(s); }, Qt::QueuedConnection);

    QObject::connect(
      &m_loot, &Loot::progress,
      this, [&](auto&& s){ setText(s); }, Qt::QueuedConnection);

    QObject::connect(
      &m_loot, &Loot::information, this,
      [&](auto&& mod, auto&& i){ setInfo(mod, i); }, Qt::QueuedConnection);

    QObject::connect(
      &m_loot, &Loot::errorMessage, this,
      [&](auto&& s){ onErrorMessage(s); }, Qt::QueuedConnection);

    QObject::connect(
      &m_loot, &Loot::error, this,
      [&](auto&& s){ onError(s); }, Qt::QueuedConnection);

    QObject::connect(
      &m_loot, &Loot::finished, this,
      [&]{ onFinished(); }, Qt::QueuedConnection);
  }

  void setText(const QString& s)
  {
    m_label->setText(s);
  }

  void setIndeterminate()
  {
    m_progress->setMaximum(0);
  }

  void addOutput(const QString& s)
  {
    const auto lines = s.split(QRegExp("[\\r\\n]"), QString::SkipEmptyParts);

    for (auto&& line : lines) {
      if (line.isEmpty()) {
        continue;
      }

      addLineOutput(line);
    }
  }

  void setInfo(const QString& mod, const QString& info)
  {
    m_core.pluginList()->addInformation(mod.toStdString().c_str(), info);
  }

  bool result() const
  {
    return m_loot.result();
  }

  void cancel()
  {
    addOutput(tr("Stopping LOOT..."));
    m_loot.cancel();
  }

  int exec() override
  {
    QDialog::exec();

    if (m_errorMessages.length() > 0) {
      QMessageBox *warn = new QMessageBox(
        QMessageBox::Warning, tr("Errors occurred"),
        m_errorMessages, QMessageBox::Ok, parentWidget());

      warn->exec();
    }

    return 0;
  }

  void onError(const QString& s)
  {
    reportError(s);
  }

private:
  OrganizerCore& m_core;
  Loot& m_loot;
  QLabel* m_label;
  QProgressBar* m_progress;
  QDialogButtonBox* m_buttons;
  QPlainTextEdit* m_output;
  QString m_lastLine;
  QString m_errorMessages;
  bool m_finished;

  void createUI()
  {
    auto* root = new QWidget(this);
    auto* ly = new QVBoxLayout(root);

    setLayout(new QVBoxLayout);
    layout()->setContentsMargins(0, 0, 0, 0);
    layout()->addWidget(root);

    m_label = new QLabel;
    ly->addWidget(m_label);

    m_progress = new QProgressBar;
    ly->addWidget(m_progress);

    m_output = new QPlainTextEdit;
    ly->addWidget(m_output);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    connect(m_buttons, &QDialogButtonBox::clicked, [&](auto* b){ onButton(b); });
    ly->addWidget(m_buttons);
  }

  void closeEvent(QCloseEvent* e) override
  {
    if (m_finished) {
      QDialog::closeEvent(e);
    } else {
      cancel();
      e->ignore();
    }
  }

  void onButton(QAbstractButton* b)
  {
    if (m_buttons->buttonRole(b) == QDialogButtonBox::RejectRole) {
      cancel();
    }
  }

  void addLineOutput(const QString& line)
  {
    if (line == m_lastLine) {
      return;
    }

    m_output->appendPlainText(line);
    m_lastLine = line;
  }

  void onFinished()
  {
    m_finished = true;
    close();
  }

  void onErrorMessage(const QString& s)
  {
    m_errorMessages += s;
  }
};


Loot::Loot()
  : m_thread(nullptr), m_cancel(false), m_result(false)
{
}

Loot::~Loot()
{
  m_thread->wait();
}

bool Loot::start(QWidget* parent, OrganizerCore& core, bool didUpdateMasterList)
{
  m_outPath = QDir::temp().absoluteFilePath("lootreport.json");

  QStringList parameters;
  parameters
    << "--game" << core.managedGame()->gameShortName()
    << "--gamePath" << QString("\"%1\"").arg(core.managedGame()->gameDirectory().absolutePath())
    << "--pluginListPath" << QString("\"%1/loadorder.txt\"").arg(core.profilePath())
    << "--out" << QString("\"%1\"").arg(m_outPath);

  if (didUpdateMasterList) {
    parameters << "--skipUpdateMasterlist";
  }

  SECURITY_ATTRIBUTES secAttributes;
  secAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
  secAttributes.bInheritHandle = TRUE;
  secAttributes.lpSecurityDescriptor = nullptr;

  env::HandlePtr readPipe, writePipe;

  {
    HANDLE read = INVALID_HANDLE_VALUE;
    HANDLE write = INVALID_HANDLE_VALUE;

    if (!::CreatePipe(&read, &write, &secAttributes, 0)) {
      log::error("failed to create stdout reroute");
    }

    readPipe.reset(read);
    writePipe.reset(write);

    if (!::SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0)) {
      log::error("failed to correctly set up the stdout reroute");
    }
  }

  core.prepareVFS();

  spawn::SpawnParameters sp;
  sp.binary = QFileInfo(qApp->applicationDirPath() + "/loot/lootcli.exe");
  sp.arguments = parameters.join(" ");
  sp.currentDirectory.setPath(qApp->applicationDirPath() + "/loot");
  sp.hooked = true;
  sp.stdOut = writePipe.get();

  m_stdout = std::move(readPipe);

  HANDLE lootHandle = spawn::startBinary(parent, sp);
  if (lootHandle == INVALID_HANDLE_VALUE) {
    emit error(tr("failed to start loot"));
    return false;
  }

  m_lootProcess.reset(lootHandle);

  core.pluginList()->clearAdditionalInformation();

  m_thread.reset(QThread::create([&]{
    lootThread();
    emit finished();
  }));

  m_thread->start();

  return true;
}

void Loot::cancel()
{
  m_cancel = true;
}

bool Loot::result() const
{
  return m_result;
}

void Loot::lootThread()
{
  try {
    m_result = false;

    if (!waitForCompletion()) {
      return;
    }

    m_result = true;
    processOutputFile();
  } catch (const std::exception &e) {
    emit error(tr("failed to run loot: %1").arg(e.what()));
  }
}

bool Loot::waitForCompletion()
{
  HANDLE waitHandle = m_lootProcess.get();
  DWORD res = ::MsgWaitForMultipleObjects(1, &waitHandle, false, 100, QS_KEY | QS_MOUSE);

  while ((res != WAIT_FAILED) && (res != WAIT_OBJECT_0)) {
    if (m_cancel) {
      ::TerminateProcess(m_lootProcess.get(), 1);
    }

    std::string lootOut = readFromPipe();
    processStdout(lootOut);

    res = ::MsgWaitForMultipleObjects(1, &waitHandle, false, 100, QS_KEY | QS_MOUSE);
  }

  const std::string remainder = readFromPipe();
  if (!remainder.empty()) {
    processStdout(remainder);
  }

  if (m_cancel) {
    return false;
  }


  // checking exit code

  DWORD exitCode = 0;

  if (!::GetExitCodeProcess(m_lootProcess.get(), &exitCode)) {
    const auto e = GetLastError();
    log::error("failed to get exit code for loot, {}", formatSystemMessage(e));
    return false;
  }

  if (exitCode != 0UL) {
    emit error(tr("Loot failed. Exit code was: %1").arg(exitCode));
    return false;
  }

  return true;
}

std::string Loot::readFromPipe()
{
  static const int chunkSize = 128;
  std::string result;

  char buffer[chunkSize + 1];
  buffer[chunkSize] = '\0';

  DWORD read = 1;
  while (read > 0) {
    if (!::ReadFile(m_stdout.get(), buffer, chunkSize, &read, nullptr)) {
      break;
    }
    if (read > 0) {
      result.append(buffer, read);
      if (read < chunkSize) {
        break;
      }
    }
  }
  return result;
}

void Loot::processStdout(const std::string &lootOut)
{
  emit output(QString::fromStdString(lootOut));

  std::vector<std::string> lines;
  boost::split(lines, lootOut, boost::is_any_of("\r\n"));

  std::regex exRequires("\"([^\"]*)\" requires \"([^\"]*)\", but it is missing\\.");
  std::regex exIncompatible("\"([^\"]*)\" is incompatible with \"([^\"]*)\", but both are present\\.");

  for (const std::string &line : lines) {
    if (line.length() > 0) {
      size_t progidx    = line.find("[progress]");
      size_t erroridx   = line.find("[error]");
      if (progidx != std::string::npos) {
        emit progress(line.substr(progidx + 11).c_str());
      } else if (erroridx != std::string::npos) {
        log::warn("{}", line);
        emit errorMessage(QString::fromStdString(
          boost::algorithm::trim_copy(line.substr(erroridx + 8)) + "\n"));
      } else {
        std::smatch match;
        if (std::regex_match(line, match, exRequires)) {
          std::string modName(match[1].first, match[1].second);
          std::string dependency(match[2].first, match[2].second);
          emit information(
            QString::fromStdString(modName),
            tr("depends on missing \"%1\"").arg(dependency.c_str()));
        } else if (std::regex_match(line, match, exIncompatible)) {
          std::string modName(match[1].first, match[1].second);
          std::string dependency(match[2].first, match[2].second);
          emit information(
            QString::fromStdString(modName),
            tr("incompatible with \"%1\"").arg(dependency.c_str()));
        } else {
          log::debug("[loot] {}", line);
        }
      }
    }
  }
}

QString jsonType(const QJsonValue& v)
{
  if (v.isUndefined()) {
    return "undefined";
  } else if (v.isNull()) {
    return "null";
  } else if (v.isArray()) {
    return "an array";
  } else if (v.isBool()) {
    return "a bool";
  } else if (v.isDouble()) {
    return "a double";
  } else if (v.isObject()) {
    return "an object";
  } else if (v.isString()) {
    return "a string";
  } else {
    return "an unknown type";
  }
}

QString jsonType(const QJsonDocument& doc)
{
  if (doc.isEmpty()) {
    return "empty";
  } else if (doc.isNull()) {
    return "null";
  } else if (doc.isArray()) {
    return "an array";
  } else if (doc.isObject()) {
    return "an object";
  } else {
    return "an unknown type";
  }
}

void Loot::processOutputFile()
{
  QFile outFile(m_outPath);
  if (!outFile.open(QIODevice::ReadOnly)) {
    logJsonError(
      "failed to open file, {} (error {})",
      outFile.errorString(), outFile.error());

    return;
  }

  QJsonParseError e;
  const QJsonDocument doc = QJsonDocument::fromJson(outFile.readAll(), &e);
  if (doc.isNull()) {
    logJsonError("invalid json, {} (error {})", e.errorString(), e.error);
    return;
  }

  if (!doc.isArray()) {
    logJsonError("root is {}, not an array", jsonType(doc));
    return;
  }

  const QJsonArray array = doc.array();

  for (auto pluginValue : array) {
    processOutputPlugin(pluginValue);
  }
}

bool Loot::processOutputPlugin(const QJsonValue& pluginValue)
{
  if (!pluginValue.isObject()) {
    logJsonError(
      "value in root array is {}, not an object", jsonType(pluginValue));
    return false;
  }

  const auto plugin = pluginValue.toObject();


  if (!plugin.contains("name")) {
    logJsonError("plugin value doesn't have a 'name' property");
    return false;
  }

  const auto pluginNameValue = plugin["name"];
  if (!pluginNameValue.isString()) {
    logJsonError(
      "plugin property 'name' is {}, not a string", jsonType(pluginNameValue));
    return false;
  }

  const auto pluginName = pluginNameValue.toString();

  processPluginMessages(pluginName, plugin);
  processPluginDirty(pluginName, plugin);

  return true;
}

bool Loot::processPluginMessages(
  const QString& pluginName, const QJsonObject& plugin)
{
  if (!plugin.contains("messages")) {
    return true;
  }

  const auto messagesValue = plugin["messages"];

  if (!messagesValue.isArray()) {
    logJsonError(
      "'messages' value for plugin '{}' is {}, not an array",
      pluginName, jsonType(messagesValue));

    return false;
  }

  const auto messages = messagesValue.toArray();


  for (auto messageValue : messages) {
    if (!messageValue.isObject()) {
      logJsonError(
        "plugin '{}' has a message that's {}, not an object",
        pluginName, jsonType(messageValue));

      continue;
    }

    processPluginMessage(pluginName, messageValue.toObject());
  }

  return true;
}

bool Loot::processPluginMessage(
  const QString& pluginName, const QJsonObject& message)
{
  const auto messageType = message["type"].toString();
  const auto messageString = message["message"].toString();

  if (messageType.isEmpty()) {
    logJsonError(
      "plugin '{}' has a message with no 'type' property", pluginName);
    return false;
  }

  if (messageString.isEmpty()) {
    logJsonError(
      "plugin '{}' has a message with no 'message' property", pluginName);
    return false;
  }

  const auto info = QString("%1: %2")
    .arg(messageType)
    .arg(messageString);

  emit information(pluginName, info);
  return true;
}


bool Loot::processPluginDirty(
  const QString& pluginName, const QJsonObject& plugin)
{
  if (!plugin.contains("dirty")) {
    return true;
  }

  const auto dirtyValue = plugin["dirty"];

  if (!dirtyValue.isArray()) {
    logJsonError(
      "'dirty' value for plugin '{}' is {}, not an array",
      pluginName, jsonType(dirtyValue));

    return false;
  }

  const auto dirty = dirtyValue.toArray();


  for (auto stringValue : dirty) {
    if (!stringValue.isString()) {
      logJsonError(
        "'dirty' value for plugin '{}' is {}, not a string",
        pluginName, jsonType(stringValue));

      continue;
    }

    const auto string = stringValue.toString();

    if (string.isEmpty()) {
      logJsonError("'dirty' string for plugin '{}' is empty", pluginName);
      continue;
    }

    emit information(pluginName, string);
  }

  return true;
}


bool runLoot(QWidget* parent, OrganizerCore& core, bool didUpdateMasterList)
{
  //m_OrganizerCore.currentProfile()->writeModlistNow();
  core.savePluginList();

  //Create a backup of the load orders w/ LOOT in name
  //to make sure that any sorting is easily undo-able.
  //Need to figure out how I want to do that.

  try {
    Loot loot;
    LootDialog dialog(parent, core, loot);

    loot.start(parent, core, didUpdateMasterList);

    dialog.setText(QObject::tr("Please wait while LOOT is running"));
    dialog.setIndeterminate();
    dialog.exec();

    return dialog.result();
  } catch (const UsvfsConnectorException &e) {
    log::debug("{}", e.what());
    return false;
  } catch (const std::exception &e) {
    reportError(QObject::tr("failed to run loot: %1").arg(e.what()));
    return false;
  }
}
