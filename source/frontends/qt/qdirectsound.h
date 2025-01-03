#ifndef DIRECTSOUND_H
#define DIRECTSOUND_H

#include <QtGlobal>

#include <vector>
#include <string>
#include <memory>

class SoundBuffer;

namespace QDirectSound
{
    struct SoundInfo
    {
        std::string streamName;
        bool running = false;
        int channels = 0;

        // in milli seconds
        int buffer = 0;
        int size = 0;

        size_t numberOfUnderruns = 0;
    };

    std::shared_ptr<SoundBuffer> iCreateDirectSoundBuffer(DWORD dwFlags, DWORD dwBufferSize, DWORD nSampleRate, int nChannels, LPCSTR pStreamName);

    void setOptions(const qint64 duration);  // in ms
    std::vector<SoundInfo> getAudioInfo();
}

#endif // DIRECTSOUND_H
