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

    virtual void VideoPresentScreen();
    virtual void FrameRefreshStatus(int drawflags);
    virtual void Initialize();
    virtual void Destroy();

    virtual int FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType);
    virtual void GetBitmap(LPCSTR lpBitmapName, LONG cb, LPVOID lpvBits);
    virtual BYTE* GetResource(WORD id, LPCSTR lpType, DWORD expectedSize);

    void SetForceRepaint(const bool force);
    void SetZoom(const int x);
    void Set43Ratio();
    bool saveScreen(const QString & filename) const;

private:
    Emulator * myEmulator;
    QMdiSubWindow * myWindow;
    bool myForceRepaint;

    QByteArray myResource;
};

#endif // QTFRAME_H
