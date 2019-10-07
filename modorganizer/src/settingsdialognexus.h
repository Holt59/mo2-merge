#ifndef SETTINGSDIALOGNEXUS_H
#define SETTINGSDIALOGNEXUS_H

#include "settings.h"
#include "settingsdialog.h"
#include "nxmaccessmanager.h"

class NexusSettingsTab : public SettingsTab
{
public:
  NexusSettingsTab(Settings& settings, SettingsDialog& dialog);
  void update();

private:
  std::unique_ptr<NexusSSOLogin> m_nexusLogin;
  std::unique_ptr<NexusKeyValidator> m_nexusValidator;

  void on_nexusConnect_clicked();
  void on_nexusManualKey_clicked();
  void on_nexusDisconnect_clicked();
  void on_clearCacheButton_clicked();
  void on_associateButton_clicked();

  void validateKey(const QString& key);
  bool setKey(const QString& key);
  bool clearKey();

  void updateNexusState();
  void updateNexusButtons();
  void updateNexusData();

  void onSSOKeyChanged(const QString& key);
  void onSSOStateChanged(NexusSSOLogin::States s, const QString& e);

  void onValidatorFinished(
    ValidationAttempt::Result r, const QString& message,
    std::optional<APIUserAccount> useR);

  void addNexusLog(const QString& s);
};

#endif // SETTINGSDIALOGNEXUS_H
