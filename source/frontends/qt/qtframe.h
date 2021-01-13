#ifndef QTFRAME_H
#define QTFRAME_H

#include "linux/linuxframe.h"
#include <memory>
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

    void SetForceRepaint(const bool force);
    void SetZoom(const int x);
    void Set43Ratio();
    bool saveScreen(const QString & filename) const;

private:
    Emulator * myEmulator;
    QMdiSubWindow * myWindow;
    bool myForceRepaint;
};

#endif // QTFRAME_H
