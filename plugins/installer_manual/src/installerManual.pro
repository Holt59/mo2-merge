#-------------------------------------------------
#
# Project created by QtCreator 2012-12-29T17:34:26
#
#-------------------------------------------------

TARGET = installerManual
TEMPLATE = lib

CONFIG += plugins
CONFIG += dll

CONFIG(release, debug|release) {
  QMAKE_CXXFLAGS += /Zi
  QMAKE_LFLAGS += /DEBUG
}

contains(QT_VERSION, "^5.*") {
	QT += widgets quick script
} else {
	QT += declarative script
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
