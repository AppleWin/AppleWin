#ifndef DIRECTSOUND_H
#define DIRECTSOUND_H

#include <QtGlobal>

#include <vector>
#include <string>

namespace QDirectSound
{
    struct SoundInfo
    {
        std::string name;
        bool running = false;
        int channels = 0;

        // in milli seconds
        int buffer = 0;
        int size = 0;

        size_t numberOfUnderruns = 0;
    };

    void setOptions(const qint64 duration);  // in ms
    std::vector<SoundInfo> getAudioInfo();
}

#endif // DIRECTSOUND_H
