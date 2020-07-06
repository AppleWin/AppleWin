#ifndef DIRECTSOUND_H
#define DIRECTSOUND_H

#include <QtGlobal>


namespace QDirectSound
{
    void start();
    void stop();
    void writeAudio();
    void setOptions(const qint32 initialSilence, const qint32 silenceDelay, const qint32 volume);
}

#endif // DIRECTSOUND_H
