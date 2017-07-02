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
    video.cpp \
    qresources.cpp

HEADERS  += qapple.h \
    video.h

FORMS    += qapple.ui \
    video.ui

RESOURCES += \
    qapple.qrc

unix: LIBS += -L$$PWD/../../../ -lappleii

INCLUDEPATH += $$PWD/../../../source
DEPENDPATH += $$PWD/../../../source
