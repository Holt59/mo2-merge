#-------------------------------------------------
#
# Project created by QtCreator 2012-12-29T17:34:26
#
#-------------------------------------------------

TARGET = installerManual
TEMPLATE = lib

CONFIG += plugins
CONFIG += dll

QT += declarative script

contains(QT_VERSION, "^5.*") {
  QT += widgets
}

DEFINES += INSTALLERMANUAL_LIBRARY

SOURCES += installermanual.cpp \
    installdialog.cpp \
    archivetree.cpp

HEADERS += installermanual.h \
    installdialog.h \
    archivetree.h

include(../plugin_template.pri)

FORMS += \
    installdialog.ui

OTHER_FILES += \
    installermanual.json
