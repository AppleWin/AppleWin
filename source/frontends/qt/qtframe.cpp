#include "StdAfx.h"
#include "qtframe.h"
#include "emulator.h"

#include "Core.h"
#include "Utilities.h"

#include <QMdiSubWindow>

QtFrame::QtFrame(Emulator * emulator, QMdiSubWindow * window) : myEmulator(emulator), myWindow(window), myForceRepaint(false)
{

}

void QtFrame::SetForceRepaint(const bool force)
{
    myForceRepaint = force;
}

void QtFrame::VideoPresentScreen()
{
    myEmulator->refreshScreen(myForceRepaint);
}

void QtFrame::FrameRefreshStatus(int drawflags)
{
    if (drawflags & DRAW_TITLE)
    {
        GetAppleWindowTitle();
        myWindow->setWindowTitle(QString::fromStdString(g_pAppTitle));
    }
}

void QtFrame::Initialize()
{
    LinuxFrame::Initialize();
    FrameRefreshStatus(DRAW_TITLE);
    myEmulator->loadVideoSettings();
    myEmulator->displayLogo();
}

void QtFrame::Destroy()
{
    LinuxFrame::Destroy();
    myEmulator->unloadVideoSettings();
}

void QtFrame::SetZoom(const int x)
{
    myEmulator->setZoom(myWindow, x);
}

void QtFrame::Set43Ratio()
{
    myEmulator->set43AspectRatio(myWindow);
}

bool QtFrame::saveScreen(const QString & filename) const
{
    return myEmulator->saveScreen(filename);
}
