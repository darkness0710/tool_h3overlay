QT       += core gui sql network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++20

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

QMAKE_CXXFLAGS += -pedantic -Wall -Wextra -Wcast-qual -Wcast-align -Wstrict-null-sentinel -Werror -Wno-unused -Wold-style-cast -Wstrict-aliasing

RC_ICONS = resources/images/main/icon.ico

VERSION = 1.1.5.0
QMAKE_TARGET_DESCRIPTION = Heroes 3 HotA Overlay

SOURCES += \
    src/aboutwindow.cpp \
    src/hotametaclient.cpp \
    src/main.cpp \
    src/maindisplay.cpp \
    src/mainwindow.cpp \
    src/memoryscanner.cpp \
    src/processhandler.cpp \
    src/resizelabel.cpp \
    src/settings.cpp \
    src/settingswindow.cpp \
    src/statisticsdatabase.cpp \
    src/statstableview.cpp \
    src/statswindow.cpp

HEADERS += \
    src/aboutwindow.h \
    src/gamestructs.h \
    src/hotametaclient.h \
    src/maindisplay.h \
    src/mainwindow.h \
    src/memoryscanner.h \
    src/processhandler.h \
    src/resizelabel.h \
    src/settings.h \
    src/settingswindow.h \
    src/statisticsdatabase.h \
    src/statstableview.h \
    src/statswindow.h

FORMS += \
    forms/mainwindow.ui \
    forms/maindisplay.ui \
    forms/aboutwindow.ui \
    forms/settingswindow.ui \
    forms/statswindow.ui

TRANSLATIONS += \
    h3overlay_en_US.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resources/resources.qrc


copyReadme.commands = $(COPY_FILE) $$shell_path($$PWD\README.md) $$shell_path($$OUT_PWD/release/)
copyLicense.commands = $(COPY_FILE) $$shell_path($$PWD\LICENSE.md) $$shell_path($$OUT_PWD/release/)
first.depends = $(first) copyReadme copyLicense
export(first.depends)
export(copyReadme.commands)
export(copyLicense.commands)
QMAKE_EXTRA_TARGETS += first copyReadme copyLicense
