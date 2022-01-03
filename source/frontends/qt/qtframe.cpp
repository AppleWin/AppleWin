#include "StdAfx.h"
#include "qtframe.h"
#include "emulator.h"

#include "Core.h"
#include "Utilities.h"
#include "Log.h"

#include "linux/resources.h"

#include <QMdiSubWindow>
#include <QFile>
#include <QMessageBox>
#include <QStandardPaths>

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

void QtFrame::Initialize(bool resetVideoState)
{
    LinuxFrame::Initialize(resetVideoState);
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

int QtFrame::FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
    QMessageBox::StandardButtons buttons = QMessageBox::Ok;
    if (uType & MB_YESNO)
    {
        buttons = QMessageBox::Yes | QMessageBox::No;
    }
    else if (uType & MB_YESNOCANCEL)
    {
        buttons = QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel;
    }

    QMessageBox::StandardButton result = QMessageBox::information(nullptr, lpCaption, lpText, buttons);

    switch (result)
    {
    case QMessageBox::Ok:
        return IDOK;
    case QMessageBox::Yes:
        return IDYES;
    case QMessageBox::No:
        return IDNO;
    case QMessageBox::Cancel:
        return IDCANCEL;
    default:
        return IDOK;
    }
}

void QtFrame::GetBitmap(LPCSTR lpBitmapName, LONG cb, LPVOID lpvBits)
{
    const std::string path = std::string(":/resources/") + lpBitmapName + ".BMP";
    QImage image = QImage(QString::fromStdString(path));

    if (image.isNull())
    {
        return LinuxFrame::GetBitmap(lpBitmapName, cb, lpvBits);
    }

    const uchar * bits = image.bits();
    const qsizetype size = image.sizeInBytes();
    const qsizetype requested = cb;

    const qsizetype copied = std::min(requested, size);

    uchar * dest = static_cast<uchar *>(lpvBits);
    for (qsizetype i = 0; i < copied; ++i)
    {
        dest[i] = ~bits[i];
    }
}

BYTE* QtFrame::GetResource(WORD id, LPCSTR lpType, DWORD expectedSize)
{
    Q_UNUSED(lpType);
    myResource.clear();

    const std::string & filename = getResourceName(id);
    const std::string path = std::string(":/resources/") + filename;

    QFile res(QString::fromStdString(path));
    if (res.exists() && res.open(QIODevice::ReadOnly))
    {
        QByteArray resource = res.readAll();
        if (resource.size() == expectedSize)
        {
            std::swap(myResource, resource);
        }
    }

    if (myResource.isEmpty())
    {
        LogFileOutput("FindResource: could not load resource %s\n", filename.c_str());
    }

    return reinterpret_cast<BYTE *>(myResource.data());
}

std::string QtFrame::Video_GetScreenShotFolder()
{
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    return pictures.toStdString() + "/";
}

void SingleStep(bool /* bReinit */)
{

}
