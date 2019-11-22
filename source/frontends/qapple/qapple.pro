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
    memorycontainer.cpp \
    preferences.cpp \
    gamepadpaddle.cpp \
    settings.cpp \
    configuration.cpp \
    qhexedit2/chunks.cpp \
    qhexedit2/commands.cpp \
    qhexedit2/qhexedit.cpp

HEADERS  += qapple.h \
    audiogenerator.h \
    emulator.h \
    loggingcategory.h \
    video.h \
    graphicscache.h \
    memorycontainer.h \
    preferences.h \
    gamepadpaddle.h \
    settings.h \
    configuration.h \
    qhexedit2/chunks.h \
    qhexedit2/commands.h \
    qhexedit2/qhexedit.h

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

