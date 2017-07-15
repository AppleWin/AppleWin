#include "memorycontainer.h"

#include "StdAfx.h"
#include "Memory.h"

MemoryContainer::MemoryContainer(QWidget *parent) :
    QTabWidget(parent)
{
    setupUi(this);

    const char * mainBase = (const char *)MemGetMainPtr(0);
    QByteArray mainMemory = QByteArray::fromRawData(mainBase, 0x10000);

    main->setReadOnly(true);
    main->setData(mainMemory);

    const char * auxBase = (const char *)MemGetAuxPtr(0);
    QByteArray auxMemory = QByteArray::fromRawData(auxBase, 0x10000);

    aux->setReadOnly(true);
    aux->setData(auxMemory);
}
