TEMPLATE = app
TARGET = starcapp

CONFIG += c++1z
CONFIG += force_debug_info
CONFIG += separate_debug_info
QT += core gui widgets

DEFINES += QT_DEPRECATED_WARNINGS

DESTDIR = ../_build/

INCLUDEPATH += ..

LIBS += -L$$DESTDIR

#include(../3rd_party/qbreakpad/qBreakpad.pri)

LIBSDIR = ../_build/libs

#
# Подключаем Crashpad
#
LIBS += -L$$LIBSDIR/ -lcrashpad_paths
INCLUDEPATH += $$PWD/../3rd_party/crashpad_paths
DEPENDPATH += $$PWD/../3rd_party/crashpad_paths

CRASHPAD_DIR = $$PWD/../3rd_party/chromium/crashpad/crashpad
EXEDIR = $$OUT_PWD/../_build

INCLUDEPATH += $$CRASHPAD_DIR
INCLUDEPATH += $$CRASHPAD_DIR/third_party/mini_chromium/mini_chromium
CONFIG(debug, debug|release) {
    INCLUDEPATH += $$CRASHPAD_DIR/out/debug/gen
}
CONFIG(release, debug|release) {
    INCLUDEPATH += $$CRASHPAD_DIR/out/release/gen
}

win32 {
    CONFIG(debug, debug|release) {
        OBJDIR = $$CRASHPAD_DIR/out/debug/obj
    }
    CONFIG(release, debug|release) {
        OBJDIR = $$CRASHPAD_DIR/out/release/obj
    }

    LIBS += -L$$OBJDIR/client -lcommon
    LIBS += -L$$OBJDIR/client -lclient
    LIBS += -L$$OBJDIR/util -lutil
    LIBS += -L$$OBJDIR/third_party/mini_chromium/mini_chromium/base -lbase

    LIBS += -lAdvapi32

    #
    # Копируем хэндлер в директорию сборки
    #
    QMAKE_POST_LINK += "mkdir \"$$shell_path($$EXEDIR)\\crashpad\""
    QMAKE_POST_LINK += "& copy /y \"$$shell_path($$CRASHPAD_DIR)\\out\\release\\crashpad_handler.exe\" \"$$shell_path($$EXEDIR)\\crashpad\\crashpad_handler.exe\""
}

linux {
    LIBS += -L$$CRASHPAD_DIR/out/release/obj/client -lcommon
    LIBS += -L$$CRASHPAD_DIR/out/release/obj/client -lclient
    LIBS += -L$$CRASHPAD_DIR/out/release/obj/util -lutil
    LIBS += -L$$CRASHPAD_DIR/out/release/obj/third_party/mini_chromium/mini_chromium/base -lbase

    #
    # Копируем хэндлер в директорию сборки
    #
    QMAKE_POST_LINK += "mkdir -p $$EXEDIR/crashpad && cp $$CRASHPAD_DIR/out/release/crashpad_handler $$EXEDIR/crashpad/crashpad_handler"
}

macx {
    LIBS += -L$$CRASHPAD_DIR/out/release/obj/client -lcommon
    LIBS += -L$$CRASHPAD_DIR/out/release/obj/client -lclient
    LIBS += -L$$CRASHPAD_DIR/out/release/obj/util -lutil
    LIBS += -L$$CRASHPAD_DIR/out/release/obj/third_party/mini_chromium/mini_chromium/base -lbase
    LIBS += -L$$CRASHPAD_DIR/out/release/obj/util -lmig_output

    LIBS += -L/usr/lib/ -lbsm
    LIBS += -framework AppKit
    LIBS += -framework Security

    #
    # Копируем хэндлер в директорию сборки
    #
    QMAKE_POST_LINK += "mkdir -p $$EXEDIR/crashpad && cp $$CRASHPAD_DIR/out/release/crashpad_handler $$EXEDIR/crashpad/crashpad_handler"
}
#

SOURCES += \
        application.cpp \
        main.cpp

HEADERS += \
    application.h

win32:RC_FILE = app.rc
macx {
    ICON = icon.icns
    QMAKE_INFO_PLIST = Info.plist
}
