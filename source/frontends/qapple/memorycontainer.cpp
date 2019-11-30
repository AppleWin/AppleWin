#include "memorycontainer.h"
#include "ui_memorycontainer.h"

#include "StdAfx.h"
#include "Memory.h"

MemoryContainer::MemoryContainer(QWidget *parent) :
    QTabWidget(parent),
    ui(new Ui::MemoryContainer)
{
    ui->setupUi(this);

    const char * mainBase = reinterpret_cast<const char *>(MemGetMainPtr(0));
    QByteArray mainMemory = QByteArray::fromRawData(mainBase, 0x10000);

    ui->main->setReadOnly(true);
    ui->main->setData(mainMemory);

    const char * auxBase = reinterpret_cast<const char *>(MemGetAuxPtr(0));
    QByteArray auxMemory = QByteArray::fromRawData(auxBase, 0x10000);

    ui->aux->setReadOnly(true);
    ui->aux->setData(auxMemory);
}
