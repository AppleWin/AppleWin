#ifndef MEMORYCONTAINER_H
#define MEMORYCONTAINER_H

#include <QTabWidget>

namespace Ui
{
    class MemoryContainer;
}

class MemoryContainer : public QTabWidget
{
    Q_OBJECT

public:
    explicit MemoryContainer(QWidget *parent = nullptr);
    ~MemoryContainer();

private:
    Ui::MemoryContainer *ui;
};

#endif // MEMORYCONTAINER_H
