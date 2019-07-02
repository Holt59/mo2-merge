#include "modinfodialognexus.h"
#include "ui_modinfodialog.h"
#include "settings.h"
#include "organizercore.h"
#include "iplugingame.h"
#include "bbcode.h"
#include <versioninfo.h>
#include <utility.h>

bool isValidModID(int id)
{
  return (id > 0);
}

NexusTab::NexusTab(
  OrganizerCore& oc, PluginContainer& plugin,
  QWidget* parent, Ui::ModInfoDialog* ui, int id) :
    ModInfoDialogTab(oc, plugin, parent, ui, id), m_requestStarted(false),
    m_loading(false)
{
  ui->modID->setValidator(new QIntValidator(ui->modID));
  ui->endorse->setVisible(core().settings().endorsementIntegration());

  connect(ui->modID, &QLineEdit::editingFinished, [&]{ onModIDChanged(); });
  connect(ui->version, &QLineEdit::editingFinished, [&]{ onVersionChanged(); });
  connect(ui->openInBrowser, &QToolButton::clicked, [&]{ onOpenLink(); });
  connect(ui->url, &QLineEdit::editingFinished, [&]{ onUrlChanged(); });
  connect(ui->endorse, &QToolButton::clicked, [&]{ onEndorse(); });
  connect(ui->refresh, &QToolButton::clicked, [&]{ onRefreshBrowser(); });

  connect(
    ui->sourceGame,
    static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
    [&]{ onSourceGameChanged(); });
}

NexusTab::~NexusTab()
{
  cleanup();
}

void NexusTab::cleanup()
{
  if (m_modConnection) {
    disconnect(m_modConnection);
    m_modConnection = {};
  }
}

void NexusTab::clear()
{
  ui->modID->clear();
  ui->sourceGame->clear();
  ui->version->clear();
  ui->browser->setPage(new NexusTabWebpage(ui->browser));
  ui->url->clear();
  setHasData(false);
}

void NexusTab::update()
{
  QScopedValueRollback loading(m_loading, true);

  clear();

  ui->modID->setText(QString("%1").arg(mod()->getNexusID()));

  QString gameName = mod()->getGameName();
  ui->sourceGame->addItem(
    core().managedGame()->gameName(),
    core().managedGame()->gameShortName());

  if (core().managedGame()->validShortNames().size() == 0) {
    ui->sourceGame->setDisabled(true);
  } else {
    for (auto game : plugin().plugins<MOBase::IPluginGame>()) {
      for (QString gameName : core().managedGame()->validShortNames()) {
        if (game->gameShortName().compare(gameName, Qt::CaseInsensitive) == 0) {
          ui->sourceGame->addItem(game->gameName(), game->gameShortName());
          break;
        }
      }
    }
  }

  ui->sourceGame->setCurrentIndex(ui->sourceGame->findData(gameName));

  auto* page = new NexusTabWebpage(ui->browser);
  ui->browser->setPage(page);

  connect(
    page, &NexusTabWebpage::linkClicked,
    [&](const QUrl& url){ MOBase::shell::OpenLink(url); });

  ui->endorse->setEnabled(
    (mod()->endorsedState() == ModInfo::ENDORSED_FALSE) ||
    (mod()->endorsedState() == ModInfo::ENDORSED_NEVER));

  setHasData(mod()->getNexusID() >= 0);
}

void NexusTab::firstActivation()
{
  updateWebpage();
}

void NexusTab::setMod(ModInfo::Ptr mod, MOShared::FilesOrigin* origin)
{
  cleanup();

  ModInfoDialogTab::setMod(mod, origin);

  m_modConnection = connect(
    mod.data(), &ModInfo::modDetailsUpdated, [&]{ onModChanged(); });
}

bool NexusTab::usesOriginFiles() const
{
  return false;
}

void NexusTab::updateVersionColor()
{
  if (mod()->getVersion() != mod()->getNewestVersion()) {
    ui->version->setStyleSheet("color: red");
    ui->version->setToolTip(tr("Current Version: %1").arg(
      mod()->getNewestVersion().canonicalString()));
  } else {
    ui->version->setStyleSheet("color: green");
    ui->version->setToolTip(tr("No update available"));
  }
}

void NexusTab::updateWebpage()
{
  const int modID = mod()->getNexusID();

  if (isValidModID(modID)) {
    const QString nexusLink = NexusInterface::instance(&plugin())
      ->getModURL(modID, mod()->getGameName());

    ui->openInBrowser->setToolTip(nexusLink);
    mod()->setURL(nexusLink);
    refreshData(modID);
  } else {
    onModChanged();
  }

  ui->version->setText(mod()->getVersion().displayString());
  ui->url->setText(mod()->getURL());
}

void NexusTab::onModChanged()
{
  m_requestStarted = false;

  const QString nexusDescription = mod()->getNexusDescription();

  QString descriptionAsHTML = R"(
<html>
  <head>
    <style class="nexus-description">
    body
    {
      font-family: sans-serif;
      font-size: 14px;
      background: #404040;
      color: #f1f1f1;
    }

    a
    {
      color: #8197ec;
      text-decoration: none;
    }
    </style>
  </head>
  <body>%1</body>
</html>)";

  if (nexusDescription.isEmpty()) {
    descriptionAsHTML = descriptionAsHTML.arg(tr(
      "<div style=\"text-align: center;\">"
      "<h1>Uh oh!</h1>"
      "<p>Sorry, there is no description available for this mod. :(</p>"
      "</div>"));

  } else {
    descriptionAsHTML = descriptionAsHTML.arg(
      BBCode::convertToHTML(nexusDescription));
  }

  ui->browser->page()->setHtml(descriptionAsHTML);
  updateVersionColor();
}

void NexusTab::onModIDChanged()
{
  if (m_loading) {
    return;
  }

  const int oldID = mod()->getNexusID();
  const int newID = ui->modID->text().toInt();

  if (oldID != newID){
    mod()->setNexusID(newID);
    mod()->setLastNexusQuery(QDateTime::fromSecsSinceEpoch(0));

    ui->browser->page()->setHtml("");

    if (isValidModID(newID)) {
      refreshData(newID);
    }
  }
}

void NexusTab::onSourceGameChanged()
{
  if (m_loading) {
    return;
  }

  for (auto game : plugin().plugins<MOBase::IPluginGame>()) {
    if (game->gameName() == ui->sourceGame->currentText()) {
      mod()->setGameName(game->gameShortName());
      mod()->setLastNexusQuery(QDateTime::fromSecsSinceEpoch(0));
      refreshData(mod()->getNexusID());
      return;
    }
  }
}

void NexusTab::onVersionChanged()
{
  if (m_loading) {
    return;
  }

  MOBase::VersionInfo version(ui->version->text());
  mod()->setVersion(version);
  updateVersionColor();
}

void NexusTab::onUrlChanged()
{
  if (m_loading) {
    return;
  }

  mod()->setURL(ui->url->text());
  mod()->setLastNexusQuery(QDateTime::fromSecsSinceEpoch(0));
}

void NexusTab::onOpenLink()
{
  const int modID = mod()->getNexusID();

  if (isValidModID(modID)) {
    const QString nexusLink = NexusInterface::instance(&plugin())
     ->getModURL(modID, mod()->getGameName());

    MOBase::shell::OpenLink(QUrl(nexusLink));
  }
}

void NexusTab::onRefreshBrowser()
{
  const auto modID = mod()->getNexusID();

  if (isValidModID(modID)) {
    mod()->setLastNexusQuery(QDateTime::fromSecsSinceEpoch(0));
    updateWebpage();
  } else {
    qInfo("Mod has no valid Nexus ID, info can't be updated.");
  }
}

void NexusTab::onEndorse()
{
  core().loggedInAction(parentWidget(), [m=mod()]{ m->endorse(true); });
}

void NexusTab::refreshData(int modID)
{
  if (tryRefreshData(modID)) {
    m_requestStarted = true;
  } else {
    onModChanged();
  }
}

bool NexusTab::tryRefreshData(int modID)
{
  if (isValidModID(modID) && !m_requestStarted) {
    if (mod()->updateNXMInfo()) {
      ui->browser->setHtml("");
      return true;
    }
  }

  return false;
}
