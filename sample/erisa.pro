QT += core

CONFIG += c++11

TARGET = erisa
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

release: DESTDIR = release
debug:   DESTDIR = debug

OBJECTS_DIR = $$DESTDIR/prebuild_s
MOC_DIR = $$DESTDIR/prebuild_s
RCC_DIR = $$DESTDIR/prebuild_s
UI_DIR = $$DESTDIR/prebuild_s

SOURCES+=   main.cpp \
    Source/decimagec.cpp \
    Source/erisacontextc.cpp \
    Source/erisamatrixc.cpp \
    Source/decerisa.cpp \
    Source/decimage.cpp \
    Source/encerisa.cpp \
    Source/encimage.cpp \
    Source/erianime.cpp \
    Source/erisacontext.cpp \
    Source/erisafile.cpp \
    Source/erisamatrix.cpp \
    Source/erisasound.cpp \
    Source/meiwrite.cpp \
    Source/mioplayer.cpp \
    Source/eslarray.cpp \
    Source/eslfile.cpp \
    Source/eslheap.cpp \
    Source/eslstring.cpp

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

HEADERS += \
    xerisa.h \
    erisamatrix.h \
    erisacontext.h \
    erisaimage.h \
    erisasound.h \
    erisafile.h \
    erisaplay.h \
    egl2d.h \
    esl.h \
    eslheap.h \
    eslarray.h \
    eslstring.h \
    eslfile.h

DISTFILES +=
