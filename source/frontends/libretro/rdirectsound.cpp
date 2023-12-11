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

  class DirectSoundGenerator : public IDirectSoundBuffer 
  {
  public:
    DirectSoundGenerator(LPCDSBUFFERDESC lpcDSBufferDesc);

    virtual HRESULT Release() override;

    void writeAudio(const size_t fps, const bool write);

    bool isRunning();
    size_t getNumberOfChannels() const;

  private:
    std::vector<int16_t> myMixerBuffer;

    void mixBuffer(const void * ptr, const size_t size);
  };

  std::unordered_map<IDirectSoundBuffer *, std::shared_ptr<DirectSoundGenerator> > activeSoundGenerators;

  DirectSoundGenerator::DirectSoundGenerator(LPCDSBUFFERDESC lpcDSBufferDesc)
    : IDirectSoundBuffer(lpcDSBufferDesc)
  {
  }

  HRESULT DirectSoundGenerator::Release()
  {
    activeSoundGenerators.erase(this);
    return IUnknown::Release();
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

  size_t DirectSoundGenerator::getNumberOfChannels() const
  {
    return myChannels;
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

IDirectSoundBuffer * iCreateDirectSoundBuffer(LPCDSBUFFERDESC lpcDSBufferDesc)
{
  std::shared_ptr<DirectSoundGenerator> generator = std::make_shared<DirectSoundGenerator>(lpcDSBufferDesc);
  DirectSoundGenerator * ptr = generator.get();
  activeSoundGenerators[ptr] = generator;
  return ptr;
}

namespace ra2
{

  void writeAudio(const size_t channels, const size_t fps)
  {
    bool found = false;
    for (const auto & it : activeSoundGenerators)
    {
      const auto & generator = it.second;
      if (generator->isRunning())
      {
        const bool selected = !found && (generator->getNumberOfChannels() == channels);
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
