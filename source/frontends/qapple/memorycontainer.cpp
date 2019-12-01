#include "memorycontainer.h"
#include "ui_memorycontainer.h"

#include "StdAfx.h"
#include "Memory.h"

#include "viewbuffer.h"

namespace
{
    void setComment(QHexMetadata* metadata, int begin, int end, const QColor &fgcolor, const QColor &bgcolor, const QString &comment)
    {
        // line and start are 0 based
        // length is both ends included

        const int width = HEX_LINE_LENGTH;
        const int firstRow = begin / width;
        const int lastRow = end / width;

        for (int row = firstRow; row <= lastRow; ++row)
        {
            int start, length;
            if (row == firstRow)
            {
                start = begin % width;
            }
            else
            {
                start = 0;
            }
            if (row == lastRow)
            {
                const int lastChar = end % width;
                length = lastChar - start;
            }
            else
            {
                length = width;
            }
            if (length > 0)
            {
                metadata->metadata(row, start, length, fgcolor, bgcolor, comment);
            }
        }
    }
}


MemoryContainer::MemoryContainer(QWidget *parent) :
    QTabWidget(parent),
    ui(new Ui::MemoryContainer)
{
    ui->setupUi(this);

    char * mainBase = reinterpret_cast<char *>(MemGetMainPtr(0));
    QHexDocument * mainDocument = QHexDocument::fromMemory<ViewBuffer>(mainBase, 0x10000, this);
    ui->main->setReadOnly(true);
    ui->main->setDocument(mainDocument);

    QHexMetadata* mainMetadata = mainDocument->metadata();
    setComment(mainMetadata, 0x0400, 0x0800, Qt::blue, Qt::yellow, "Text Video Page 1");
    setComment(mainMetadata, 0x0800, 0x0C00, Qt::black, Qt::yellow, "Text Video Page 2");
    setComment(mainMetadata, 0x2000, 0x4000, Qt::blue, Qt::yellow, "HiRes Video Page 1");
    setComment(mainMetadata, 0x4000, 0x6000, Qt::black, Qt::yellow, "hiRes Video Page 2");

    char * auxBase = reinterpret_cast<char *>(MemGetAuxPtr(0));
    QHexDocument * auxDocument = QHexDocument::fromMemory<ViewBuffer>(auxBase, 0x10000, this);
    ui->aux->setReadOnly(true);
    ui->aux->setDocument(auxDocument);

    QHexMetadata* auxMetadata = auxDocument->metadata();
    setComment(auxMetadata, 0x0400, 0x0800, Qt::blue, Qt::yellow, "Text Video Page 1");
    setComment(auxMetadata, 0x0800, 0x0C00, Qt::black, Qt::yellow, "Text Video Page 2");
    setComment(auxMetadata, 0x2000, 0x4000, Qt::blue, Qt::yellow, "HiRes Video Page 1");
    setComment(auxMetadata, 0x4000, 0x6000, Qt::black, Qt::yellow, "hiRes Video Page 2");
}
