#include "memorycontainer.h"
#include "ui_memorycontainer.h"

#include "StdAfx.h"
#include "Memory.h"
#include "Debugger/Debugger_Types.h"

#include "viewbuffer.h"

namespace
{
    void setComment(QHexView* view, qint64 begin, qint64 end, const QColor &fgcolor, const QColor &bgcolor, const QString &comment)
    {
        view->setMetadata(begin, end, fgcolor, bgcolor, comment);
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
    ui->main->setReadOnly(true);
    ui->main->setDocument(mainDocument);

    setComment(ui->main, 0x0400, 0x0800, Qt::blue, Qt::yellow, "Text Video Page 1");
    setComment(ui->main, 0x0800, 0x0C00, Qt::black, Qt::yellow, "Text Video Page 2");
    setComment(ui->main, 0x2000, 0x4000, Qt::blue, Qt::yellow, "HiRes Video Page 1");
    setComment(ui->main, 0x4000, 0x6000, Qt::black, Qt::yellow, "HiRes Video Page 2");

    setComment(ui->main, 0xC000, 0xC100, Qt::white, Qt::blue, "Soft Switches");
    setComment(ui->main, 0xC100, 0xC800, Qt::white, Qt::red, "Peripheral Card Memory");
    setComment(ui->main, 0xF800, 0x10000, Qt::white, Qt::red, "System Monitor");

    // aux memory
    char * auxBase = reinterpret_cast<char *>(MemGetAuxPtr(0));
    QHexDocument * auxDocument = QHexDocument::fromMemory<ViewBuffer>(auxBase, _6502_MEM_LEN, this);
    ui->aux->setReadOnly(true);
    ui->aux->setDocument(auxDocument);

    setComment(ui->aux, 0x0400, 0x0800, Qt::blue, Qt::yellow, "Text Video Page 1");
    setComment(ui->aux, 0x0800, 0x0C00, Qt::black, Qt::yellow, "Text Video Page 2");
    setComment(ui->aux, 0x2000, 0x4000, Qt::blue, Qt::yellow, "HiRes Video Page 1");
    setComment(ui->aux, 0x4000, 0x6000, Qt::black, Qt::yellow, "HiRes Video Page 2");
}

MemoryContainer::~MemoryContainer()
{
    delete ui;
}
