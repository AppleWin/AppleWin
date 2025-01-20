#include "StdAfx.h"

#include "qapple.h"
#include <QApplication>
#include <QTimer>
#include <QCommandLineParser>

#include "linux/version.h"
#include "applicationname.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QApplication::setOrganizationName(ORGANIZATION_NAME);
    QApplication::setApplicationName(APPLICATION_NAME);
    const QString qversion = QString::fromStdString(getVersion());
    QApplication::setApplicationVersion(qversion);

    QCommandLineParser parser;
    parser.setApplicationDescription("Qt Apple Emulator");
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption runOption("r", "start immeditaely");
    parser.addOption(runOption);

    const QCommandLineOption logStateOption("load-state", "load state file", "file");
    parser.addOption(logStateOption);

    parser.process(app);

    QApple w;

    const bool run = parser.isSet(runOption);
    const QString stateFile = parser.value(logStateOption);
    if (!stateFile.isEmpty())
    {
        w.loadStateFile(stateFile);
    }

    w.show();

    if (run)
    {
        QTimer::singleShot(0, &w, SLOT(startEmulator()));
    }

    return app.exec();
}
