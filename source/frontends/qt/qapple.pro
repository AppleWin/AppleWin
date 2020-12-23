#-------------------------------------------------
#
# Project created by QtCreator 2017-07-02T20:14:02
#
#-------------------------------------------------

QT       += core gui multimedia widgets gamepad

TARGET = qapple
TEMPLATE = app


SOURCES += main.cpp\
    QHexView/document/buffer/qhexbuffer.cpp \
    QHexView/document/buffer/qmemorybuffer.cpp \
    QHexView/document/buffer/qmemoryrefbuffer.cpp \
    QHexView/document/commands/hexcommand.cpp \
    QHexView/document/commands/insertcommand.cpp \
    QHexView/document/commands/removecommand.cpp \
    QHexView/document/commands/replacecommand.cpp \
    QHexView/document/qhexcursor.cpp \
    QHexView/document/qhexdocument.cpp \
    QHexView/document/qhexmetadata.cpp \
    QHexView/document/qhexrenderer.cpp \
    QHexView/qhexview.cpp \
    loggingcategory.cpp \
    options.cpp \
    qapple.cpp \
    qdirectsound.cpp \
    qresources.cpp \
    emulator.cpp \
    registry.cpp \
    video.cpp \
    memorycontainer.cpp \
    preferences.cpp \
    gamepadpaddle.cpp \
    viewbuffer.cpp

HEADERS  += qapple.h \
    QHexView/document/buffer/qhexbuffer.h \
    QHexView/document/buffer/qmemorybuffer.h \
    QHexView/document/buffer/qmemoryrefbuffer.h \
    QHexView/document/commands/hexcommand.h \
    QHexView/document/commands/insertcommand.h \
    QHexView/document/commands/removecommand.h \
    QHexView/document/commands/replacecommand.h \
    QHexView/document/qhexcursor.h \
    QHexView/document/qhexdocument.h \
    QHexView/document/qhexmetadata.h \
    QHexView/document/qhexrenderer.h \
    QHexView/qhexview.h \
    emulator.h \
    loggingcategory.h \
    options.h \
    qdirectsound.h \
    registry.h \
    video.h \
    memorycontainer.h \
    preferences.h \
    gamepadpaddle.h \
    viewbuffer.h

FORMS    += qapple.ui \
    emulator.ui \
    memorycontainer.ui \
    preferences.ui

RESOURCES += \
    qapple.qrc

unix: LIBS += -L$$PWD/../../../build/source -lappleii

INCLUDEPATH += $$PWD/../../../source
DEPENDPATH += $$PWD/../../../source

unix: LIBS += -levdev

unix:QMAKE_RPATHDIR += $ORIGIN/../..
