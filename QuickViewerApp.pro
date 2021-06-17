#-------------------------------------------------
#
# Project created by QtCreator 2019-10-21T16:01:56
#
#-------------------------------------------------

QT += core gui widgets network websockets
QTPLUGIN += qwebp

TARGET = QuickViewerApp
TEMPLATE = app

win32:RC_ICONS += src/favicon.ico

QMAKE_LFLAGS_RELEASE += -static -static-libgcc

CONFIG += c++11

INCLUDEPATH += src

SOURCES += \
    src/input_simulator.cpp \
    src/qv_main.cpp \
    src/qv_mainwindow.cpp \
    src/screen_capture.cpp \
    src/ws_handler.cpp

HEADERS += \
    src/input_simulator.h \
    src/qv_mainwindow.h \
    src/screen_capture.h \
    src/ws_handler.h

FORMS += \
    src/qv_mainwindow.ui

#RESOURCES += \
#    src/res.qrc

linux-g++: \
    LIBS += -lX11 -lXtst

# === build parameters ===
win32: OS_SUFFIX = win32
linux-g++: OS_SUFFIX = linux

CONFIG(debug, debug|release) {
    BUILD_FLAG = debug
    LIB_SUFFIX = d
} else {
    BUILD_FLAG = release
}

RCC_DIR = $${PWD}/build/$${BUILD_FLAG}
UI_DIR = $${PWD}/build/$${BUILD_FLAG}
UI_HEADERS_DIR = $${PWD}/build/$${BUILD_FLAG}
UI_SOURCES_DIR = $${PWD}/build/$${BUILD_FLAG}
MOC_DIR = $${PWD}/build/$${BUILD_FLAG}
OBJECTS_DIR = $${PWD}/build/$${BUILD_FLAG}
DESTDIR = $${PWD}/bin/$${BUILD_FLAG}
