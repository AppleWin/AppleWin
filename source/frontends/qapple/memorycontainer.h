#ifndef MEMORYCONTAINER_H
#define MEMORYCONTAINER_H

#include "ui_memorycontainer.h"

class MemoryContainer : public QTabWidget, private Ui::MemoryContainer
{
    Q_OBJECT

public:
    explicit MemoryContainer(QWidget *parent = 0);
};

#endif // MEMORYCONTAINER_H
