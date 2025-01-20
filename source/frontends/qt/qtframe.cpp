#include "StdAfx.h"
#include "qtframe.h"
#include "emulator.h"
#include "qdirectsound.h"

#include "Core.h"
#include "Utilities.h"

#include "apple2roms_data.h"

#include <QMdiSubWindow>
#include <QFile>
#include <QMessageBox>
#include <QStandardPaths>

QtFrame::QtFrame(Emulator *emulator, QMdiSubWindow *window)
    : LinuxFrame(true)
    , myEmulator(emulator)
    , myWindow(window)
    , myForceRepaint(false)
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

bool QtFrame::saveScreen(const QString &filename) const
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

std::pair<const unsigned char *, unsigned int> QtFrame::GetResourceData(WORD id) const
{
    const auto it = apple2roms::data.find(id);
    if (it == apple2roms::data.end())
    {
        throw std::runtime_error("Cannot locate resource: " + std::to_string(id));
    }
    return it->second;
}

std::string QtFrame::Video_GetScreenShotFolder() const
{
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    return pictures.toStdString() + "/";
}

std::shared_ptr<SoundBuffer> QtFrame::CreateSoundBuffer(
    uint32_t dwBufferSize, uint32_t nSampleRate, int nChannels, const char *pszVoiceName)
{
    const auto buffer = QDirectSound::iCreateDirectSoundBuffer(dwBufferSize, nSampleRate, nChannels, pszVoiceName);
    return buffer;
}

void SingleStep(bool /* bReinit */)
{
}
