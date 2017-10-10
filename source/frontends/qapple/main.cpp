#include "qapple.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QApplication::setOrganizationName("AndSoft");
    QApplication::setApplicationName("QAppleEmulator");

    QApple w;
    w.show();

    return a.exec();
}
