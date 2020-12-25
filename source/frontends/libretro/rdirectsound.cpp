#include "frontends/libretro/rdirectsound.h"
#include "frontends/libretro/environment.h"

#include "linux/windows/dsound.h"

#include <unordered_map>
#include <memory>
#include <iostream>
#include <iomanip>
#include <cmath>

namespace
{

  class DirectSoundGenerator
  {
  public:
    DirectSoundGenerator(IDirectSoundBuffer * buffer);

    void writeAudio(const size_t ms);
    void playSilence(const size_t ms);

  private:
    IDirectSoundBuffer * myBuffer;

    std::vector<int16_t> myMixerBuffer;

    size_t myBytesPerSecond;

    bool isRunning() const;

    void mixBuffer(const void * ptr, const size_t size);
  };


  std::unordered_map<IDirectSoundBuffer *, std::shared_ptr<DirectSoundGenerator>> activeSoundGenerators;

  DirectSoundGenerator::DirectSoundGenerator(IDirectSoundBuffer * buffer)
    : myBuffer(buffer)
  {
    myBytesPerSecond = myBuffer->channels * myBuffer->sampleRate * sizeof(int16_t);
  }

  bool DirectSoundGenerator::isRunning() const
  {
    DWORD dwStatus;
    myBuffer->GetStatus(&dwStatus);
    if (!(dwStatus & DSBSTATUS_PLAYING))
    {
      return false;
    }
    else
    {
      // for now, just play speaker audio
      return myBuffer->channels == 1;
    }
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
      myMixerBuffer.resize(2 * size);
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

    audio_batch_cb(myMixerBuffer.data(), frames);
  }

  void DirectSoundGenerator::playSilence(const size_t ms)
  {
    if (!isRunning())
    {
      return;
    }

    const size_t frames = ms * myBuffer->sampleRate / 1000;
    const size_t bytesToWrite = frames * sizeof(int16_t) * 2;
    myMixerBuffer.resize(bytesToWrite);
    std::fill(myMixerBuffer.begin(), myMixerBuffer.end(), 0);
    audio_batch_cb(myMixerBuffer.data(), frames);
  }

  void DirectSoundGenerator::writeAudio(const size_t ms)
  {
    // this is autostart as we only do for the palying buffers
    // and AW might activate one later
    if (!isRunning())
    {
      return;
    }

    const size_t bytesToRead = ms * myBytesPerSecond / 1000;

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

namespace RDirectSound
{

  void writeAudio(const size_t ms)
  {
    for (auto & it : activeSoundGenerators)
    {
      const std::shared_ptr<DirectSoundGenerator> & generator = it.second;
      generator->writeAudio(ms);
    }
  }

  void playSilence(const size_t ms)
  {
    for (auto & it : activeSoundGenerators)
    {
      const std::shared_ptr<DirectSoundGenerator> & generator = it.second;
      generator->playSilence(ms);
    }
  }

  void bufferStatusCallback(bool active, unsigned occupancy, bool underrun_likely)
  {
    if (active)
    {
      // I am not sure this is any useful
      if (underrun_likely || occupancy < 40)
      {
	log_cb(RETRO_LOG_INFO, "RA2: %s occupancy = %d, underrun_likely = %d\n", __FUNCTION__, occupancy, underrun_likely);
	playSilence(10);
      }
    }
  }

}
