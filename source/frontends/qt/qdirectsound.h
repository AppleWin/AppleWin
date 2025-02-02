#ifndef DIRECTSOUND_H
#define DIRECTSOUND_H

#include <QtGlobal>
#include <QAudio>

#include <vector>
#include <string>
#include <memory>

class SoundBuffer;

namespace QDirectSound
{
    struct SoundInfo
    {
        std::string voiceName;
        bool running = false;
        QAudio::State state = QAudio::StoppedState;
        int channels = 0;

        // in milli seconds
        int buffer = 0;
        int size = 0;
        int volume = 0; // 0 - 100

        size_t numberOfUnderruns = 0;
    };

    std::shared_ptr<SoundBuffer> iCreateDirectSoundBuffer(
        uint32_t dwBufferSize, uint32_t nSampleRate, int nChannels, const char *pszVoiceName);

    void setOptions(const qint64 duration); // in ms
    std::vector<SoundInfo> getAudioInfo();
} // namespace QDirectSound

#endif // DIRECTSOUND_H
