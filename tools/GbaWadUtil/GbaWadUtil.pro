# A console app: run with no arguments (or with -gui) it shows a window instead.
QT += widgets svg

CONFIG += c++17 console
CONFIG -= app_bundle

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        dehacked.cpp \
        gui/buildpage.cpp \
        gui/mainwindow.cpp \
        gui/spacechart.cpp \
        gui/ui.cpp \
        gusbank.cpp \
        infotables.cpp \
        main.cpp \
        mapgame.cpp \
        mus2mid.cpp \
        options.cpp \
        sizereport.cpp \
        usedlumps.cpp \
        wadfile.cpp \
        wadprocessor.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# Release builds go straight into GBADoom's GbaWadUtil/ folder, next to
# data/ (gbadoom.wad, the GUS patches) and the build scripts that the exe needs.
CONFIG(release, debug|release): DESTDIR = $$PWD/../../GbaWadUtil

HEADERS += \
    dehacked.h \
    doomtypes.h \
    gui/buildpage.h \
    gui/gui.h \
    gui/mainwindow.h \
    gui/spacechart.h \
    gui/ui.h \
    gusbank.h \
    infotables.h \
    mapgame.h \
    mus2mid.h \
    options.h \
    sizereport.h \
    wadfile.h \
    wadprocessor.h \
    ../../include/deh_text.h \
    ../../include/doom_iwad.h \
    ../../include/gusbank.h \
    ../../include/info.h \
    ../../include/sounds.h

DISTFILES += \
    ../../GbaWadUtil/data/gbadoom.wad
