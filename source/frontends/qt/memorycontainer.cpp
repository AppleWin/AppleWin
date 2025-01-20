#include "memorycontainer.h"
#include "ui_memorycontainer.h"

#include "StdAfx.h"
#include "Memory.h"
#include "Debugger/Debugger_Types.h"

#include "viewbuffer.h"

namespace
{
    struct Metadata_t
    {
        qint64 begin;
        qint64 end;
        QColor fgcolor;
        QColor bgcolor;
        QString comment;
    };

    void setMetadata(QHexView *view, const Metadata_t &comment)
    {
        view->setMetadata(comment.begin, comment.end, comment.fgcolor, comment.bgcolor, comment.comment);
    }
} // namespace

MemoryContainer::MemoryContainer(QWidget *parent)
    : QTabWidget(parent)
    , ui(new Ui::MemoryContainer)
{
    ui->setupUi(this);

    // main memory
    char *mainBase = reinterpret_cast<char *>(MemGetMainPtr(0));
    QHexDocument *mainDocument = QHexDocument::fromMemory<ViewBuffer>(mainBase, _6502_MEM_LEN, this);
    ui->main->setReadOnly(true);
    ui->main->setDocument(mainDocument);

    const Metadata_t mainMetadata[] = {
        {0x0400, 0x0800, Qt::blue, Qt::yellow, "Text Video Page 1"},
        {0x0800, 0x0C00, Qt::black, Qt::yellow, "Text Video Page 2"},
        {0x2000, 0x4000, Qt::blue, Qt::yellow, "HiRes Video Page 1"},
        {0x4000, 0x6000, Qt::black, Qt::yellow, "HiRes Video Page 2"},
        {0xC000, 0xC100, Qt::white, Qt::blue, "Soft Switches"},
        {0xC100, 0xC800, Qt::white, Qt::red, "Peripheral Card Memory"},
        {0xF800, 0x10000, Qt::white, Qt::red, "System Monitor"},
    };

    for (const auto &metadata : mainMetadata)
    {
        setMetadata(ui->main, metadata);
    }

    // aux memory
    char *auxBase = reinterpret_cast<char *>(MemGetAuxPtr(0));
    QHexDocument *auxDocument = QHexDocument::fromMemory<ViewBuffer>(auxBase, _6502_MEM_LEN, this);
    ui->aux->setReadOnly(true);
    ui->aux->setDocument(auxDocument);

    const Metadata_t auxMetadata[] = {
        {0x0400, 0x0800, Qt::blue, Qt::yellow, "Text Video Page 1"},
        {0x0800, 0x0C00, Qt::black, Qt::yellow, "Text Video Page 2"},
        {0x2000, 0x4000, Qt::blue, Qt::yellow, "HiRes Video Page 1"},
        {0x4000, 0x6000, Qt::black, Qt::yellow, "HiRes Video Page 2"},
    };

    for (const auto &metadata : auxMetadata)
    {
        setMetadata(ui->aux, metadata);
    }
}

MemoryContainer::~MemoryContainer()
{
    delete ui;
}
