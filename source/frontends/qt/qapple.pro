#-------------------------------------------------
#
# Project created by QtCreator 2017-07-02T20:14:02
#
#-------------------------------------------------

QT       += core gui multimedia widgets gamepad

TARGET = qapple
TEMPLATE = app


SOURCES += main.cpp\
    audioinfo.cpp \
    loggingcategory.cpp \
    options.cpp \
    qapple.cpp \
    qdirectsound.cpp \
    emulator.cpp \
    configuration.cpp \
    qvideo.cpp \
    memorycontainer.cpp \
    preferences.cpp \
    gamepadpaddle.cpp \
    viewbuffer.cpp \
    qtframe.cpp

HEADERS  += qapple.h \
    audioinfo.h \
    emulator.h \
    loggingcategory.h \
    options.h \
    qdirectsound.h \
    configuration.h \
    qvideo.h \
    memorycontainer.h \
    preferences.h \
    gamepadpaddle.h \
    viewbuffer.h \
    qtframe.h \
    applicationname.h

FORMS    += qapple.ui \
    audioinfo.ui \
    emulator.ui \
    memorycontainer.ui \
    preferences.ui

RESOURCES += \
    qapple.qrc

LIBS += -L$$PWD/../../../build/source -lappleii
LIBS += -L$$PWD/../../../build/source/frontends/common2 -lcommon2
LIBS += -L$$PWD/../../../build/source/linux/libwindows -lwindows
LIBS += -L$$PWD/../../../build/source/frontends/qt/QHexView -lqhexview-lib
LIBS += -levdev -lminizip -lz -lyaml -lslirp -lpcap

INCLUDEPATH += $$PWD/../../../source
INCLUDEPATH += $$PWD/../../../source/linux/libwindows
INCLUDEPATH += $$PWD/QHexView
INCLUDEPATH += /usr/include/minizip

QMAKE_RPATHDIR += $ORIGIN/../..
