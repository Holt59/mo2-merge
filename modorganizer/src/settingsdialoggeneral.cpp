#include "settingsdialoggeneral.h"
#include "ui_settingsdialog.h"
#include "appconfig.h"
#include "categoriesdialog.h"
#include <questionboxmemory.h>

using MOBase::QuestionBoxMemory;

GeneralTab::GeneralTab(Settings *m_parent, SettingsDialog &m_dialog)
  : SettingsTab(m_parent, m_dialog)
{
  addLanguages();
  {
    QString languageCode = m_parent->language();
    int currentID        = ui->languageBox->findData(languageCode);
    // I made a mess. :( Most languages are stored with only the iso country
    // code (2 characters like "de") but chinese
    // with the exact language variant (zh_TW) so I have to search for both
    // variants
    if (currentID == -1) {
      currentID = ui->languageBox->findData(languageCode.mid(0, 2));
    }
    if (currentID != -1) {
      ui->languageBox->setCurrentIndex(currentID);
    }
  }

  addStyles();
  {
    int currentID = ui->styleBox->findData(
      m_Settings.value("Settings/style", "").toString());
    if (currentID != -1) {
      ui->styleBox->setCurrentIndex(currentID);
    }
  }
  /* verision using palette only works with fusion theme for some stupid reason...
  m_overwritingBtn->setAutoFillBackground(true);
  m_overwrittenBtn->setAutoFillBackground(true);
  m_containsBtn->setAutoFillBackground(true);
  m_containedBtn->setAutoFillBackground(true);
  m_overwritingBtn->setPalette(QPalette(m_parent->modlistOverwritingLooseColor()));
  m_overwrittenBtn->setPalette(QPalette(m_parent->modlistOverwrittenLooseColor()));
  m_containsBtn->setPalette(QPalette(m_parent->modlistContainsPluginColor()));
  m_containedBtn->setPalette(QPalette(m_parent->pluginListContainedColor()));
  QPalette palette1 = m_overwritingBtn->palette();
  QPalette palette2 = m_overwrittenBtn->palette();
  QPalette palette3 = m_containsBtn->palette();
  QPalette palette4 = m_containedBtn->palette();
  palette1.setColor(QPalette::Background, m_parent->modlistOverwritingLooseColor());
  palette2.setColor(QPalette::Background, m_parent->modlistOverwrittenLooseColor());
  palette3.setColor(QPalette::Background, m_parent->modlistContainsPluginColor());
  palette4.setColor(QPalette::Background, m_parent->pluginListContainedColor());
  m_overwritingBtn->setPalette(palette1);
  m_overwrittenBtn->setPalette(palette2);
  m_containsBtn->setPalette(palette3);
  m_containedBtn->setPalette(palette4);
  */

  //version with stylesheet
  setButtonColor(ui->overwritingBtn, m_parent->modlistOverwritingLooseColor());
  setButtonColor(ui->overwrittenBtn, m_parent->modlistOverwrittenLooseColor());
  setButtonColor(ui->overwritingArchiveBtn, m_parent->modlistOverwritingArchiveColor());
  setButtonColor(ui->overwrittenArchiveBtn, m_parent->modlistOverwrittenArchiveColor());
  setButtonColor(ui->containsBtn, m_parent->modlistContainsPluginColor());
  setButtonColor(ui->containedBtn, m_parent->pluginListContainedColor());

  setOverwritingColor(m_parent->modlistOverwritingLooseColor());
  setOverwrittenColor(m_parent->modlistOverwrittenLooseColor());
  setOverwritingArchiveColor(m_parent->modlistOverwritingArchiveColor());
  setOverwrittenArchiveColor(m_parent->modlistOverwrittenArchiveColor());
  setContainsColor(m_parent->modlistContainsPluginColor());
  setContainedColor(m_parent->pluginListContainedColor());

  ui->compactBox->setChecked(m_parent->compactDownloads());
  ui->showMetaBox->setChecked(m_parent->metaDownloads());
  ui->usePrereleaseBox->setChecked(m_parent->usePrereleases());
  ui->colorSeparatorsBox->setChecked(m_parent->colorSeparatorScrollbar());

  QObject::connect(ui->overwritingArchiveBtn, &QPushButton::clicked, [&]{ on_overwritingArchiveBtn_clicked(); });
  QObject::connect(ui->overwritingBtn, &QPushButton::clicked, [&]{ on_overwritingBtn_clicked(); });
  QObject::connect(ui->overwrittenArchiveBtn, &QPushButton::clicked, [&]{ on_overwrittenArchiveBtn_clicked(); });
  QObject::connect(ui->overwrittenBtn, &QPushButton::clicked, [&]{ on_overwrittenBtn_clicked(); });
  QObject::connect(ui->containedBtn, &QPushButton::clicked, [&]{ on_containedBtn_clicked(); });
  QObject::connect(ui->containsBtn, &QPushButton::clicked, [&]{ on_containsBtn_clicked(); });
  QObject::connect(ui->categoriesBtn, &QPushButton::clicked, [&]{ on_categoriesBtn_clicked(); });
  QObject::connect(ui->resetColorsBtn, &QPushButton::clicked, [&]{ on_resetColorsBtn_clicked(); });
  QObject::connect(ui->resetDialogsButton, &QPushButton::clicked, [&]{ on_resetDialogsButton_clicked(); });
}

void GeneralTab::update()
{
  QString oldLanguage = m_parent->language();
  QString newLanguage = ui->languageBox->itemData(ui->languageBox->currentIndex()).toString();
  if (newLanguage != oldLanguage) {
    m_Settings.setValue("Settings/language", newLanguage);
    emit m_parent->languageChanged(newLanguage);
  }

  QString oldStyle = m_Settings.value("Settings/style", "").toString();
  QString newStyle = ui->styleBox->itemData(ui->styleBox->currentIndex()).toString();
  if (oldStyle != newStyle) {
    m_Settings.setValue("Settings/style", newStyle);
    emit m_parent->styleChanged(newStyle);
  }

  m_Settings.setValue("Settings/overwritingLooseFilesColor", getOverwritingColor());
  m_Settings.setValue("Settings/overwrittenLooseFilesColor", getOverwrittenColor());
  m_Settings.setValue("Settings/overwritingArchiveFilesColor", getOverwritingArchiveColor());
  m_Settings.setValue("Settings/overwrittenArchiveFilesColor", getOverwrittenArchiveColor());
  m_Settings.setValue("Settings/containsPluginColor", getContainsColor());
  m_Settings.setValue("Settings/containedColor", getContainedColor());
  m_Settings.setValue("Settings/compact_downloads", ui->compactBox->isChecked());
  m_Settings.setValue("Settings/meta_downloads", ui->showMetaBox->isChecked());
  m_Settings.setValue("Settings/use_prereleases", ui->usePrereleaseBox->isChecked());
  m_Settings.setValue("Settings/colorSeparatorScrollbars", ui->colorSeparatorsBox->isChecked());
}

void GeneralTab::addLanguages()
{
  std::vector<std::pair<QString, QString>> languages;

  QDirIterator langIter(QCoreApplication::applicationDirPath() + "/translations", QDir::Files);
  QString pattern = QString::fromStdWString(AppConfig::translationPrefix()) +  "_([a-z]{2,3}(_[A-Z]{2,2})?).qm";
  QRegExp exp(pattern);
  while (langIter.hasNext()) {
    langIter.next();
    QString file = langIter.fileName();
    if (exp.exactMatch(file)) {
      QString languageCode = exp.cap(1);
      QLocale locale(languageCode);
      QString languageString = QString("%1 (%2)").arg(locale.nativeLanguageName()).arg(locale.nativeCountryName());  //QLocale::languageToString(locale.language());
      if (locale.language() == QLocale::Chinese) {
        if (languageCode == "zh_TW") {
          languageString = "Chinese (traditional)";
        } else {
          languageString = "Chinese (simplified)";
        }
      }
      languages.push_back(std::make_pair(QString("%1").arg(languageString), exp.cap(1)));
      //languageBox->addItem(QString("%1").arg(languageString), exp.cap(1));
    }
  }
  if (!ui->languageBox->findText("English")) {
    languages.push_back(std::make_pair(QString("English"), QString("en_US")));
    //languageBox->addItem("English", "en_US");
  }
  std::sort(languages.begin(), languages.end());
  for (const auto &lang : languages) {
    ui->languageBox->addItem(lang.first, lang.second);
  }
}

void GeneralTab::addStyles()
{
  ui->styleBox->addItem("None", "");
  ui->styleBox->addItem("Fusion", "Fusion");

  QDirIterator langIter(QCoreApplication::applicationDirPath() + "/" + QString::fromStdWString(AppConfig::stylesheetsPath()), QStringList("*.qss"), QDir::Files);
  while (langIter.hasNext()) {
    langIter.next();
    QString style = langIter.fileName();
    ui->styleBox->addItem(style, style);
  }
}

void GeneralTab::resetDialogs()
{
  QuestionBoxMemory::resetDialogs();
}

void GeneralTab::setButtonColor(QPushButton *button, const QColor &color)
{
  button->setStyleSheet(
    QString("QPushButton {"
      "background-color: rgba(%1, %2, %3, %4);"
      "color: %5;"
      "border: 1px solid;"
      "padding: 3px;"
      "}")
    .arg(color.red())
    .arg(color.green())
    .arg(color.blue())
    .arg(color.alpha())
    .arg(Settings::getIdealTextColor(color).name())
  );
};

void GeneralTab::on_containsBtn_clicked()
{
  QColor result = QColorDialog::getColor(m_ContainsColor, parentWidget(), "Color Picker: Mod contains selected plugin", QColorDialog::ShowAlphaChannel);
  if (result.isValid()) {
    m_ContainsColor = result;
    setButtonColor(ui->containsBtn, result);
  }
}

void GeneralTab::on_containedBtn_clicked()
{
  QColor result = QColorDialog::getColor(m_ContainedColor, parentWidget(), "ColorPicker: Plugin is Contained in selected Mod", QColorDialog::ShowAlphaChannel);
  if (result.isValid()) {
    m_ContainedColor = result;
    setButtonColor(ui->containedBtn, result);
  }
}

void GeneralTab::on_overwrittenBtn_clicked()
{
  QColor result = QColorDialog::getColor(m_OverwrittenColor, parentWidget(), "ColorPicker: Is overwritten (loose files)", QColorDialog::ShowAlphaChannel);
  if (result.isValid()) {
    m_OverwrittenColor = result;
    setButtonColor(ui->overwrittenBtn, result);
  }
}

void GeneralTab::on_overwritingBtn_clicked()
{
  QColor result = QColorDialog::getColor(m_OverwritingColor, parentWidget(), "ColorPicker: Is overwriting (loose files)", QColorDialog::ShowAlphaChannel);
  if (result.isValid()) {
    m_OverwritingColor = result;
    setButtonColor(ui->overwritingBtn, result);
  }
}

void GeneralTab::on_overwrittenArchiveBtn_clicked()
{
  QColor result = QColorDialog::getColor(m_OverwrittenArchiveColor, parentWidget(), "ColorPicker: Is overwritten (archive files)", QColorDialog::ShowAlphaChannel);
  if (result.isValid()) {
    m_OverwrittenArchiveColor = result;
    setButtonColor(ui->overwrittenArchiveBtn, result);
  }
}

void GeneralTab::on_overwritingArchiveBtn_clicked()
{
  QColor result = QColorDialog::getColor(m_OverwritingArchiveColor, parentWidget(), "ColorPicker: Is overwriting (archive files)", QColorDialog::ShowAlphaChannel);
  if (result.isValid()) {
    m_OverwritingArchiveColor = result;
    setButtonColor(ui->overwritingArchiveBtn, result);
  }
}

void GeneralTab::on_resetColorsBtn_clicked()
{
  m_OverwritingColor = QColor(255, 0, 0, 64);
  m_OverwrittenColor = QColor(0, 255, 0, 64);
  m_OverwritingArchiveColor = QColor(255, 0, 255, 64);
  m_OverwrittenArchiveColor = QColor(0, 255, 255, 64);
  m_ContainsColor = QColor(0, 0, 255, 64);
  m_ContainedColor = QColor(0, 0, 255, 64);

  setButtonColor(ui->overwritingBtn, m_OverwritingColor);
  setButtonColor(ui->overwrittenBtn, m_OverwrittenColor);
  setButtonColor(ui->overwritingArchiveBtn, m_OverwritingArchiveColor);
  setButtonColor(ui->overwrittenArchiveBtn, m_OverwrittenArchiveColor);
  setButtonColor(ui->containsBtn, m_ContainsColor);
  setButtonColor(ui->containedBtn, m_ContainedColor);
}

void GeneralTab::on_resetDialogsButton_clicked()
{
  if (QMessageBox::question(parentWidget(), QObject::tr("Confirm?"),
    QObject::tr("This will make all dialogs show up again where you checked the \"Remember selection\"-box. Continue?"),
    QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    resetDialogs();
  }
}

void GeneralTab::on_categoriesBtn_clicked()
{
  CategoriesDialog dialog(parentWidget());
  if (dialog.exec() == QDialog::Accepted) {
    dialog.commitChanges();
  }
}
