#ifndef DIRECTSOUND_H
#define DIRECTSOUND_H

#include <QtGlobal>


namespace QDirectSound
{
    void writeAudio();
    void setOptions(const qint32 initialSilence);
}

#endif // DIRECTSOUND_H
