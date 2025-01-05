#ifndef QTFRAME_H
#define QTFRAME_H

#include "linux/linuxframe.h"
#include <memory>
#include <QByteArray>
#include <QString>

class Emulator;
class QMdiSubWindow;

class QtFrame : public LinuxFrame
{
public:
    QtFrame(Emulator * emulator, QMdiSubWindow * window);

    void VideoPresentScreen() override;
    void FrameRefreshStatus(int drawflags) override;
    void Initialize(bool resetVideoState) override;
    void Destroy() override;

    int FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType) override;
    std::string Video_GetScreenShotFolder() const override;

    std::shared_ptr<SoundBuffer> CreateSoundBuffer(uint32_t dwBufferSize, uint32_t nSampleRate, int nChannels, const char* pszVoiceName) override;

    void SetForceRepaint(const bool force);
    void SetZoom(const int x);
    void Set43Ratio();
    bool saveScreen(const QString & filename) const;

private:
    Emulator * myEmulator;
    QMdiSubWindow * myWindow;
    bool myForceRepaint;

    std::pair<const unsigned char *, unsigned int> GetResourceData(WORD id) const override;
};

#endif // QTFRAME_H
