#ifndef INSTALLER_FOMOD_POSTDIALOG_H
#define INSTALLER_FOMOD_POSTDIALOG_H

#include "ui_installer_fomod_csharp_postdialog.h"

#include <QTextEdit>

/**
 * @brief Dialog for the installation of a simple archive
 * a simple archive is one that doesn't require any manual changes to work correctly
 **/
class InstallerFomodPostDialog : public QDialog
{
  Q_OBJECT

public:
  /**
   * @brief constructor
   *
   * @param preset suggested name for the mod
   * @param parent parent widget
   **/
  explicit InstallerFomodPostDialog(QWidget* parent = 0) :
    QDialog(parent), ui(new Ui::FomodCSharpPostDialog) {
    ui->setupUi(this);
    setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));
  }

  ~InstallerFomodPostDialog() { }

  void setIniSettings(std::map<QString, std::map<std::pair<QString, QString>, QString>> const& settings) {
    for (auto p : settings) {
      QTextEdit* widget = new QTextEdit(this);

      // Add all the settings:
      QString cSection;
      for (auto p2 : p.second) {
        if (cSection != p2.first.first) {
          if (!cSection.isEmpty()) {
            widget->append("");
          }
          cSection = p2.first.first;
          widget->append("[" + cSection + "]");
        }
        widget->append(p2.first.second + "=" + p2.second + "");
      }


      widget->setReadOnly(true);
      ui->tabWidget->addTab(widget, p.first);
    }
  }

  void addCreatedFile(QString filepath) {
    ui->listWidget->addItem(filepath.replace("/", QDir::separator()));
  }

private slots:

  void on_okBtn_clicked() {
    this->accept();
  }

private:
  std::unique_ptr<Ui::FomodCSharpPostDialog> ui;
};

#endif