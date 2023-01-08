#include "frontends/libretro/rdirectsound.h"
#include "frontends/libretro/environment.h"

#include "windows.h"
#include "linux/linuxinterface.h"

#include <unordered_map>
#include <memory>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>

namespace
{

  // we can only run 1 generator at a time
  // 1 is for speaker (2 would be Mockingboard)
  const size_t ourChannels = 1;

  class DirectSoundGenerator
  {
  public:
    DirectSoundGenerator(IDirectSoundBuffer * buffer);

    void writeAudio(const size_t ms);
    void playSilence(const size_t ms);

    bool isRunning() const;
    size_t getNumberOfChannels() const;

  private:
    IDirectSoundBuffer * myBuffer;

    std::vector<int16_t> myMixerBuffer;

    size_t myBytesPerSecond;

    void mixBuffer(const void * ptr, const size_t size);
  };

  std::unordered_map<IDirectSoundBuffer *, std::shared_ptr<DirectSoundGenerator>> activeSoundGenerators;

  std::shared_ptr<DirectSoundGenerator> findRunningGenerator(const size_t channels)
  {
    for (auto & it : activeSoundGenerators)
    {
      const std::shared_ptr<DirectSoundGenerator> & generator = it.second;
      if (generator->isRunning() && generator->getNumberOfChannels() == channels)
      {
	return generator;
      }
    }
    return std::shared_ptr<DirectSoundGenerator>();
  }

  DirectSoundGenerator::DirectSoundGenerator(IDirectSoundBuffer * buffer)
    : myBuffer(buffer)
  {
    myBytesPerSecond = myBuffer->channels * myBuffer->sampleRate * sizeof(int16_t);
  }

  bool DirectSoundGenerator::isRunning() const
  {
    DWORD dwStatus;
    myBuffer->GetStatus(&dwStatus);
    if (dwStatus & DSBSTATUS_PLAYING)
    {
      return true;
    }
    else
    {
      return false;
    }
  }

  size_t DirectSoundGenerator::getNumberOfChannels() const
  {
    return myBuffer->channels;
  }

  void DirectSoundGenerator::mixBuffer(const void * ptr, const size_t size)
  {
    const int16_t frames = size / (sizeof(int16_t) * myBuffer->channels);
    const int16_t * data = static_cast<const int16_t *>(ptr);

    if (myBuffer->channels == 2)
    {
      myMixerBuffer.assign(data, data + frames * myBuffer->channels);
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

    const double logVolume = myBuffer->GetLogarithmicVolume();
    // same formula as QAudio::convertVolume()
    const double linVolume = logVolume > 0.99 ? 1.0 : -std::log(1.0 - logVolume) / std::log(100.0);
    const int16_t rvolume = int16_t(linVolume * 128);

    for (int16_t & sample : myMixerBuffer)
    {
      sample = (sample * rvolume) / 128;
    }

    ra2::audio_batch_cb(myMixerBuffer.data(), frames);
  }

  void DirectSoundGenerator::playSilence(const size_t ms)
  {
    if (!isRunning())
    {
      return;
    }

    const size_t frames = ms * myBuffer->sampleRate / 1000;
    myMixerBuffer.resize(2 * frames);
    std::fill(myMixerBuffer.begin(), myMixerBuffer.end(), 0);
    ra2::audio_batch_cb(myMixerBuffer.data(), frames);
  }

  void DirectSoundGenerator::writeAudio(const size_t ms)
  {
    // this is autostart as we only do for the palying buffers
    // and AW might activate one later
    if (!isRunning())
    {
      return;
    }

    const size_t frames = ms * myBuffer->sampleRate / 1000;
    const size_t bytesToRead = frames * myBuffer->channels * sizeof(int16_t);

    LPVOID lpvAudioPtr1, lpvAudioPtr2;
    DWORD dwAudioBytes1, dwAudioBytes2;
    myBuffer->Read(bytesToRead, &lpvAudioPtr1, &dwAudioBytes1, &lpvAudioPtr2, &dwAudioBytes2);

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

void registerSoundBuffer(IDirectSoundBuffer * buffer)
{
  const std::shared_ptr<DirectSoundGenerator> generator = std::make_shared<DirectSoundGenerator>(buffer);
  activeSoundGenerators[buffer] = generator;
}

void unregisterSoundBuffer(IDirectSoundBuffer * buffer)
{
  const auto it = activeSoundGenerators.find(buffer);
  if (it != activeSoundGenerators.end())
  {
    activeSoundGenerators.erase(it);
  }
}

namespace ra2
{

  void writeAudio(const size_t ms)
  {
    const auto generator = findRunningGenerator(ourChannels);
    if (generator)
    {
      generator->writeAudio(ms);
    }
  }

  void playSilence(const size_t ms)
  {
    const auto generator = findRunningGenerator(ourChannels);
    if (generator)
    {
      generator->playSilence(ms);
    }
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
        // log_cb(RETRO_LOG_INFO, "RA2: %s occupancy = %d, underrun_likely = %d\n", __FUNCTION__, occupancy, underrun_likely);
        lastOccupancy = occupancy;
      }
    }
  }

}
