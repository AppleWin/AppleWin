#ifndef DIRECTSOUND_H
#define DIRECTSOUND_H

#include <QtGlobal>


namespace QDirectSound
{
    void start();
    void stop();
    void writeAudio();
    void setOptions(const qint32 initialSilence);
}

#endif // DIRECTSOUND_H
