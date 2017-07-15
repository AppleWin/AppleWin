#-------------------------------------------------
#
# Project created by QtCreator 2017-07-02T20:14:02
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = qapple
TEMPLATE = app


SOURCES += main.cpp\
        qapple.cpp \
    qresources.cpp \
    emulator.cpp \
    video.cpp \
    graphicscache.cpp \
    chunks.cpp \
    commands.cpp \
    qhexedit.cpp \
    memorycontainer.cpp

HEADERS  += qapple.h \
    emulator.h \
    video.h \
    graphicscache.h \
    chunks.h \
    commands.h \
    qhexedit.h \
    memorycontainer.h

FORMS    += qapple.ui \
    emulator.ui \
    memorycontainer.ui

RESOURCES += \
    qapple.qrc

unix: LIBS += -L$$PWD/../../../ -lappleii

INCLUDEPATH += $$PWD/../../../source
DEPENDPATH += $$PWD/../../../source

unix: LIBS += -levdev
