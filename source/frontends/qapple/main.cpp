#include "qapple.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApple w;
    w.show();

    return a.exec();
}
