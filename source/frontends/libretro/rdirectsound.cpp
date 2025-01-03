#include "StdAfx.h"
#include "frontends/libretro/rdirectsound.h"
#include "frontends/libretro/environment.h"

#include "linux/linuxsoundbuffer.h"

#include <unordered_set>
#include <memory>
#include <cmath>
#include <vector>

namespace
{

  ra2::AudioSource getAudioSourceFromName(const std::string & name)
  {
    // These are the strings used in DSGetSoundBuffer

    if (name == "Spkr")
    {
      return ra2::AudioSource::SPEAKER;
    }

    if (name == "MB")
    {
      return ra2::AudioSource::MOCKINGBOARD;
    }

    if (name == "SSI263")
    {
      return ra2::AudioSource::SSI263;
    }

    // something new, just ignore it
    return ra2::AudioSource::UNKNOWN;
  }

  class DirectSoundGenerator : public LinuxSoundBuffer
  {
  public:
    DirectSoundGenerator(DWORD dwBufferSize, DWORD nSampleRate, int nChannels, LPCSTR pStreamName);
    virtual ~DirectSoundGenerator() override;

    void writeAudio(const size_t fps, const bool write);

    bool isRunning();

    ra2::AudioSource getSource() const;

  private:
    const ra2::AudioSource myAudioSource;
    std::vector<int16_t> myMixerBuffer;

    void mixBuffer(const void * ptr, const size_t size);
  };

  std::unordered_set<DirectSoundGenerator *> activeSoundGenerators;

  DirectSoundGenerator::DirectSoundGenerator(DWORD dwBufferSize, DWORD nSampleRate, int nChannels, LPCSTR pStreamName)
    : LinuxSoundBuffer(dwBufferSize, nSampleRate, nChannels, pStreamName)
    , myAudioSource(getAudioSourceFromName(myStreamName))
  {
  }

  DirectSoundGenerator::~DirectSoundGenerator()
  {
    activeSoundGenerators.erase(this);
  }

  bool DirectSoundGenerator::isRunning()
  {
    DWORD dwStatus;
    GetStatus(&dwStatus);
    if (dwStatus & DSBSTATUS_PLAYING)
    {
      return true;
    }
    else
    {
      return false;
    }
  }

  ra2::AudioSource DirectSoundGenerator::getSource() const
  {
    return myAudioSource;
  }

  void DirectSoundGenerator::mixBuffer(const void * ptr, const size_t size)
  {
    const int16_t frames = size / (sizeof(int16_t) * myChannels);
    const int16_t * data = static_cast<const int16_t *>(ptr);

    if (myChannels == 2)
    {
      myMixerBuffer.assign(data, data + frames * myChannels);
    }
    else
    {
      myMixerBuffer.resize(2 * frames);
      for (int16_t i = 0; i < frames; ++i)
      {
        myMixerBuffer[i * 2] = data[i];
        myMixerBuffer[i * 2 + 1] = data[i];
      }
    }

    const double logVolume = GetLogarithmicVolume();
    // same formula as QAudio::convertVolume()
    const double linVolume = logVolume > 0.99 ? 1.0 : -std::log(1.0 - logVolume) / std::log(100.0);
    const int16_t rvolume = int16_t(linVolume * 128);

    for (int16_t & sample : myMixerBuffer)
    {
      sample = (sample * rvolume) / 128;
    }

    ra2::audio_batch_cb(myMixerBuffer.data(), frames);
  }

  void DirectSoundGenerator::writeAudio(const size_t fps, const bool write)
  {
    const size_t frames = mySampleRate / fps;
    const size_t bytesToRead = frames * myChannels * sizeof(int16_t);

    LPVOID lpvAudioPtr1, lpvAudioPtr2;
    DWORD dwAudioBytes1, dwAudioBytes2;
    // always read to keep AppleWin audio algorithms working correctly.
    Read(bytesToRead, &lpvAudioPtr1, &dwAudioBytes1, &lpvAudioPtr2, &dwAudioBytes2);

    if (write)
    {
      if (lpvAudioPtr1 && dwAudioBytes1)
      {
        mixBuffer(lpvAudioPtr1, dwAudioBytes1);
      }
      if (lpvAudioPtr2 && dwAudioBytes2)
      {
        mixBuffer(lpvAudioPtr2, dwAudioBytes2);
      }
    }
  }

}

namespace ra2
{

  std::shared_ptr<SoundBuffer> iCreateDirectSoundBuffer(DWORD dwFlags, DWORD dwBufferSize, DWORD nSampleRate, int nChannels, LPCSTR pStreamName)
  {
    std::shared_ptr<DirectSoundGenerator> generator = std::make_shared<DirectSoundGenerator>(dwBufferSize, nSampleRate, nChannels, pStreamName);
    DirectSoundGenerator * ptr = generator.get();
    activeSoundGenerators.insert(ptr);
    return generator;
  }

  void writeAudio(const AudioSource selectedSource, const size_t fps)
  {
    bool found = false;
    for (const auto & it : activeSoundGenerators)
    {
      const auto & generator = it;
      if (generator->isRunning())
      {
        const bool selected = !found && (selectedSource == generator->getSource());
        // we still read audio from all buffers
        // to keep AppleWin audio generation woking correctly
        // but only write on the selected one
        generator->writeAudio(fps, selected);
        // TODO: implement an algorithm to merge 2 channels (speaker + mockingboard)
        found = found || selected;
      }
    }
    // TODO: if found = false, we should probably write some silence
  }

  void bufferStatusCallback(bool active, unsigned occupancy, bool underrun_likely)
  {
    if (active)
    {
      // I am not sure this is any useful
      static unsigned lastOccupancy = 0;
      const int diff = std::abs(int(lastOccupancy) - int(occupancy));
      if (diff >= 5)
      {
        // this is very verbose
        log_cb(RETRO_LOG_INFO, "RA2: %s occupancy = %d, underrun_likely = %d\n", __FUNCTION__, occupancy, underrun_likely);
        lastOccupancy = occupancy;
      }
    }
  }

}
