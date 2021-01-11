#include "memorycontainer.h"
#include "ui_memorycontainer.h"

#include "StdAfx.h"
#include "Memory.h"
#include "Debugger/DebugDefs.h"

#include "viewbuffer.h"

namespace
{
    void setComment(QHexMetadata* metadata, qint64 begin, qint64 end, const QColor &fgcolor, const QColor &bgcolor, const QString &comment)
    {
        metadata->metadata(begin, end, fgcolor, bgcolor, comment);
    }
}


MemoryContainer::MemoryContainer(QWidget *parent) :
    QTabWidget(parent),
    ui(new Ui::MemoryContainer)
{
    ui->setupUi(this);

    // main memory
    char * mainBase = reinterpret_cast<char *>(MemGetMainPtr(0));
    QHexDocument * mainDocument = QHexDocument::fromMemory<ViewBuffer>(mainBase, _6502_MEM_LEN, this);
    mainDocument->setHexLineWidth(16);
    ui->main->setReadOnly(true);
    ui->main->setDocument(mainDocument);

    QHexMetadata* mainMetadata = mainDocument->metadata();
    setComment(mainMetadata, 0x0400, 0x0800, Qt::blue, Qt::yellow, "Text Video Page 1");
    setComment(mainMetadata, 0x0800, 0x0C00, Qt::black, Qt::yellow, "Text Video Page 2");
    setComment(mainMetadata, 0x2000, 0x4000, Qt::blue, Qt::yellow, "HiRes Video Page 1");
    setComment(mainMetadata, 0x4000, 0x6000, Qt::black, Qt::yellow, "HiRes Video Page 2");

    setComment(mainMetadata, 0xC000, 0xC100, Qt::white, Qt::blue, "Soft Switches");
    setComment(mainMetadata, 0xC100, 0xC800, Qt::white, Qt::red, "Peripheral Card Memory");
    setComment(mainMetadata, 0xF800, 0x10000, Qt::white, Qt::red, "System Monitor");

    // aux memory
    char * auxBase = reinterpret_cast<char *>(MemGetAuxPtr(0));
    QHexDocument * auxDocument = QHexDocument::fromMemory<ViewBuffer>(auxBase, _6502_MEM_LEN, this);
    auxDocument->setHexLineWidth(16);
    ui->aux->setReadOnly(true);
    ui->aux->setDocument(auxDocument);

    QHexMetadata* auxMetadata = auxDocument->metadata();
    setComment(auxMetadata, 0x0400, 0x0800, Qt::blue, Qt::yellow, "Text Video Page 1");
    setComment(auxMetadata, 0x0800, 0x0C00, Qt::black, Qt::yellow, "Text Video Page 2");
    setComment(auxMetadata, 0x2000, 0x4000, Qt::blue, Qt::yellow, "HiRes Video Page 1");
    setComment(auxMetadata, 0x4000, 0x6000, Qt::black, Qt::yellow, "HiRes Video Page 2");
}

MemoryContainer::~MemoryContainer()
{
    delete ui;
}
