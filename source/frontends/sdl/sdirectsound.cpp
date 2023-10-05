#include "StdAfx.h"
#include "frontends/sdl/sdirectsound.h"

#include "windows.h"
#include "linux/linuxinterface.h"

#include "SoundCore.h"

#include <SDL.h>

#include <unordered_map>
#include <memory>
#include <iostream>
#include <iomanip>

namespace
{

  size_t getBytesPerSecond(const SDL_AudioSpec & spec)
  {
    const size_t bitsPerSample = spec.format & SDL_AUDIO_MASK_BITSIZE;
    const size_t bytesPerFrame = spec.channels * bitsPerSample / 8;
    return spec.freq * bytesPerFrame;
  }

  size_t nextPowerOf2(size_t n)
  {
    size_t k = 1;
    while (k < n)
      k *= 2;
    return k;
  }

  void printAudioDeviceErrorOnce()
  {
    static bool once = false;
    if (!once)
    {
      std::cerr << "SDL_OpenAudioDevice: " << SDL_GetError() << std::endl;
      once = true;
    }
  }

  class DirectSoundGenerator : public IDirectSoundBuffer
  {
  public:
    DirectSoundGenerator(LPCDSBUFFERDESC lpcDSBufferDesc);
    virtual ~DirectSoundGenerator() override;
    virtual HRESULT Release() override;

    void stop();
    void writeAudio(const char * deviceName, const size_t ms);
    void resetUnderruns();

    void printInfo();
    sa2::SoundInfo getInfo();

  private:
    static void staticAudioCallback(void* userdata, uint8_t* stream, int len);

    void audioCallback(uint8_t* stream, int len);

    std::vector<uint8_t> myMixerBuffer;

    SDL_AudioDeviceID myAudioDevice;
    SDL_AudioSpec myAudioSpec;

    size_t myBytesPerSecond;

    void close();
    bool isRunning() const;

    uint8_t * mixBufferTo(uint8_t * stream);
  };

  std::unordered_map<IDirectSoundBuffer *, std::shared_ptr<DirectSoundGenerator> > activeSoundGenerators;

  void DirectSoundGenerator::staticAudioCallback(void* userdata, uint8_t* stream, int len)
  {
    DirectSoundGenerator * generator = static_cast<DirectSoundGenerator *>(userdata);
    return generator->audioCallback(stream, len);
  }

  void DirectSoundGenerator::audioCallback(uint8_t* stream, int len)
  {
    LPVOID lpvAudioPtr1, lpvAudioPtr2;
    DWORD dwAudioBytes1, dwAudioBytes2;
    const size_t bytesRead = Read(len, &lpvAudioPtr1, &dwAudioBytes1, &lpvAudioPtr2, &dwAudioBytes2);

    myMixerBuffer.resize(bytesRead);

    uint8_t * dest = myMixerBuffer.data();
    if (lpvAudioPtr1 && dwAudioBytes1)
    {
      memcpy(dest, lpvAudioPtr1, dwAudioBytes1);
      dest += dwAudioBytes1;
    }
    if (lpvAudioPtr2 && dwAudioBytes2)
    {
      memcpy(dest, lpvAudioPtr2, dwAudioBytes2);
      dest += dwAudioBytes2;
    }

    stream = mixBufferTo(stream);

    const size_t gap = len - bytesRead;
    if (gap)
    {
      memset(stream, myAudioSpec.silence, gap);
    }
  }

  DirectSoundGenerator::DirectSoundGenerator(LPCDSBUFFERDESC lpcDSBufferDesc)
    : IDirectSoundBuffer(lpcDSBufferDesc)
    , myAudioDevice(0)
    , myBytesPerSecond(0)
  {
    SDL_zero(myAudioSpec);
  }

  DirectSoundGenerator::~DirectSoundGenerator()
  {
    stop();
  }

  HRESULT DirectSoundGenerator::Release()
  {
    activeSoundGenerators.erase(this);
    return DS_OK;
  }

  bool DirectSoundGenerator::isRunning() const
  {
    return myAudioDevice;
  }

  void DirectSoundGenerator::printInfo()
  {
    if (isRunning())
    {
      const DWORD bytesInBuffer = GetBytesInBuffer();
      std::cerr << "Channels: " << (int)myAudioSpec.channels;
      std::cerr << ", buffer: " << std::setw(6) << bytesInBuffer;
      const double time = double(bytesInBuffer) / myBytesPerSecond * 1000;
      std::cerr << ", " << std::setw(8) << time << " ms";
      std::cerr << ", underruns: " << std::setw(10) << GetBufferUnderruns() << std::endl;
    }
  }

  sa2::SoundInfo DirectSoundGenerator::getInfo()
  {
    sa2::SoundInfo info;
    info.running = isRunning();
    info.channels = myAudioSpec.channels;
    info.volume = GetLogarithmicVolume();
    info.numberOfUnderruns = GetBufferUnderruns();

    if (info.running && myBytesPerSecond > 0)
    {
      const DWORD bytesInBuffer = GetBytesInBuffer();
      const float coeff = 1.0 / myBytesPerSecond;
      info.buffer = bytesInBuffer * coeff;
      info.size = myBufferSize * coeff;
    }

    return info;
  }

  void DirectSoundGenerator::resetUnderruns()
  {
    ResetUnderrruns();
  }

  void DirectSoundGenerator::stop()
  {
    if (myAudioDevice)
    {
      SDL_PauseAudioDevice(myAudioDevice, 1);
      SDL_CloseAudioDevice(myAudioDevice);
      myAudioDevice = 0;
    }
  }

  uint8_t * DirectSoundGenerator::mixBufferTo(uint8_t * stream)
  {
    // we could copy ADJUST_VOLUME from SDL_mixer.c and avoid all copying and (rare) race conditions
    const double logVolume = GetLogarithmicVolume();
    // same formula as QAudio::convertVolume()
    const double linVolume = logVolume > 0.99 ? 1.0 : -std::log(1.0 - logVolume) / std::log(100.0);
    const uint8_t svolume = uint8_t(linVolume * SDL_MIX_MAXVOLUME);

    const size_t len = myMixerBuffer.size();
    memset(stream, 0, len);
    SDL_MixAudioFormat(stream, myMixerBuffer.data(), myAudioSpec.format, len, svolume);
    return stream + len;
  }

  void DirectSoundGenerator::writeAudio(const char * deviceName, const size_t ms)
  {
    // this is autostart as we only do for the palying buffers
    // and AW might activate one later
    if (myAudioDevice)
    {
      return;
    }

    DWORD dwStatus;
    GetStatus(&dwStatus);
    if (!(dwStatus & DSBSTATUS_PLAYING))
    {
      return;
    }

    SDL_AudioSpec want;
    SDL_zero(want);

    want.freq = mySampleRate;
    want.format = AUDIO_S16LSB;
    want.channels = myChannels;
    want.samples = std::min<size_t>(MAX_SAMPLES, nextPowerOf2(mySampleRate * ms / 1000));
    want.callback = staticAudioCallback;
    want.userdata = this;
    myAudioDevice = SDL_OpenAudioDevice(deviceName, 0, &want, &myAudioSpec, 0);

    if (myAudioDevice)
    {
      myBytesPerSecond = getBytesPerSecond(myAudioSpec);

      SDL_PauseAudioDevice(myAudioDevice, 0);
    }
    else
    {
      printAudioDeviceErrorOnce();
    }
  }

}

IDirectSoundBuffer * iCreateDirectSoundBuffer(LPCDSBUFFERDESC lpcDSBufferDesc)
{
  std::shared_ptr<DirectSoundGenerator> generator = std::make_shared<DirectSoundGenerator>(lpcDSBufferDesc);
  IDirectSoundBuffer * buffer = generator.get();
  activeSoundGenerators[buffer] = generator;
  return buffer;
}

namespace sa2
{

  void stopAudio()
  {
    for (auto & it : activeSoundGenerators)
    {
      const auto generator = it.second;
      generator->stop();
    }
  }

  void writeAudio(const char * deviceName, const size_t ms)
  {
    for (auto & it : activeSoundGenerators)
    {
      const auto generator = it.second;
      generator->writeAudio(deviceName, ms);
    }
  }

  void printAudioInfo()
  {
    for (auto & it : activeSoundGenerators)
    {
      const auto generator = it.second;
      generator->printInfo();
    }
  }

  void resetUnderruns()
  {
    for (auto & it : activeSoundGenerators)
    {
      const auto generator = it.second;
      generator->resetUnderruns();
    }
  }

  std::vector<SoundInfo> getAudioInfo()
  {
    std::vector<SoundInfo> info;
    info.reserve(activeSoundGenerators.size());

    for (auto & it : activeSoundGenerators)
    {
      const auto & generator = it.second;
      info.push_back(generator->getInfo());
    }

    return info;
  }

}
