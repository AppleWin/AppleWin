#-------------------------------------------------
#
# Project created by QtCreator 2017-07-02T20:14:02
#
#-------------------------------------------------

QT       += core gui multimedia widgets gamepad

TARGET = qapple
TEMPLATE = app


SOURCES += main.cpp\
    audiogenerator.cpp \
    loggingcategory.cpp \
    qapple.cpp \
    qresources.cpp \
    emulator.cpp \
    video.cpp \
    graphicscache.cpp \
    chunks.cpp \
    commands.cpp \
    qhexedit.cpp \
    memorycontainer.cpp \
    preferences.cpp \
    gamepadpaddle.cpp \
    settings.cpp \
    configuration.cpp

HEADERS  += qapple.h \
    audiogenerator.h \
    emulator.h \
    loggingcategory.h \
    video.h \
    graphicscache.h \
    chunks.h \
    commands.h \
    qhexedit.h \
    memorycontainer.h \
    preferences.h \
    gamepadpaddle.h \
    settings.h \
    configuration.h

FORMS    += qapple.ui \
    emulator.ui \
    memorycontainer.ui \
    preferences.ui

RESOURCES += \
    qapple.qrc

unix: LIBS += -L$$PWD/../../ -lappleii

INCLUDEPATH += $$PWD/../../../source
DEPENDPATH += $$PWD/../../../source

unix: LIBS += -levdev

unix:QMAKE_RPATHDIR += $ORIGIN/../..

