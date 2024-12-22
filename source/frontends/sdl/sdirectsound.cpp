#include "StdAfx.h"
#include "frontends/sdl/sdirectsound.h"
#include "frontends/sdl/utils.h"
#include "frontends/common2/programoptions.h"

#include "windows.h"
#include "linux/linuxinterface.h"

#include "Core.h"
#include "SoundCore.h"
#include "Log.h"

#include <SDL.h>

#include <unordered_map>
#include <memory>
#include <iostream>
#include <iomanip>

namespace
{

  // these have to come from EmulatorOptions
  std::string audioDeviceName;
  size_t audioBuffer = 0;

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

  class DirectSoundGenerator : public IDirectSoundBuffer
  {
  public:
    DirectSoundGenerator(LPCDSBUFFERDESC lpcDSBufferDesc, const char * deviceName, const size_t ms);
    virtual ~DirectSoundGenerator() override;
    virtual HRESULT Release() override;

    virtual HRESULT Stop() override;
    virtual HRESULT Play( DWORD dwReserved1, DWORD dwReserved2, DWORD dwFlags ) override;

    void printInfo();
    sa2::SoundInfo getInfo();

  private:
    static void staticAudioCallback(void* userdata, uint8_t* stream, int len);

    void audioCallback(uint8_t* stream, int len);

    std::vector<uint8_t> myMixerBuffer;

    SDL_AudioDeviceID myAudioDevice;
    SDL_AudioSpec myAudioSpec;

    size_t myBytesPerSecond;

    uint8_t * mixBufferTo(uint8_t * stream);
  };

  std::unordered_map<DirectSoundGenerator *, std::shared_ptr<DirectSoundGenerator> > activeSoundGenerators;

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

  DirectSoundGenerator::DirectSoundGenerator(LPCDSBUFFERDESC lpcDSBufferDesc, const char * deviceName, const size_t ms)
    : IDirectSoundBuffer(lpcDSBufferDesc)
    , myAudioDevice(0)
    , myBytesPerSecond(0)
  {
    SDL_zero(myAudioSpec);

    SDL_AudioSpec want;
    SDL_zero(want);

    _ASSERT(ms > 0);

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
    }
    else
    {
      throw std::runtime_error(sa2::decorateSDLError("SDL_OpenAudioDevice"));
    }
  }

  DirectSoundGenerator::~DirectSoundGenerator()
  {
    SDL_PauseAudioDevice(myAudioDevice, 1);
    SDL_CloseAudioDevice(myAudioDevice);
  }

  HRESULT DirectSoundGenerator::Release()
  {
    activeSoundGenerators.erase(this);  // this will force the destructor
    return IUnknown::Release();
  }

  HRESULT DirectSoundGenerator::Stop()
  {
    const HRESULT res = IDirectSoundBuffer::Stop();
    SDL_PauseAudioDevice(myAudioDevice, 1);
    return res;
  }
  
  HRESULT DirectSoundGenerator::Play( DWORD dwReserved1, DWORD dwReserved2, DWORD dwFlags )
  {
    const HRESULT res = IDirectSoundBuffer::Play(dwReserved1, dwReserved2, dwFlags);
    SDL_PauseAudioDevice(myAudioDevice, 0);
    return res;
  }

  void DirectSoundGenerator::printInfo()
  {
    const uint32_t bytesInBuffer = GetBytesInBuffer();
    std::cerr << "Channels: " << (int)myAudioSpec.channels;
    std::cerr << ", buffer: " << std::setw(6) << bytesInBuffer;
    const double time = double(bytesInBuffer) / myBytesPerSecond * 1000;
    std::cerr << ", " << std::setw(8) << time << " ms";
    std::cerr << ", underruns: " << std::setw(10) << GetBufferUnderruns() << std::endl;
  }

  sa2::SoundInfo DirectSoundGenerator::getInfo()
  {
    DWORD dwStatus;
    GetStatus(&dwStatus);

    sa2::SoundInfo info;
    info.name = myName;
    info.running = dwStatus & DSBSTATUS_PLAYING;
    info.channels = myChannels;
    info.volume = GetLogarithmicVolume();
    info.numberOfUnderruns = GetBufferUnderruns();

    if (info.running && myBytesPerSecond > 0)
    {
      const uint32_t bytesInBuffer = GetBytesInBuffer();
      const float coeff = 1.0 / myBytesPerSecond;
      info.buffer = bytesInBuffer * coeff;
      info.size = myBufferSize * coeff;
    }

    return info;
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

}

IDirectSoundBuffer * iCreateDirectSoundBuffer(LPCDSBUFFERDESC lpcDSBufferDesc)
{
  try
  {
    const char * deviceName = audioDeviceName.empty() ? nullptr : audioDeviceName.c_str();

    std::shared_ptr<DirectSoundGenerator> generator = std::make_shared<DirectSoundGenerator>(lpcDSBufferDesc, deviceName, audioBuffer);
    DirectSoundGenerator * ptr = generator.get();
    activeSoundGenerators[ptr] = generator;
    return ptr;
  }
  catch (const std::exception & e)
  {
    // once this fails, no point in trying again next time
    g_bDisableDirectSound = true;
    g_bDisableDirectSoundMockingboard = true;
    LogOutput("IDirectSoundBuffer: %s\n", e.what());
    return nullptr;
  }
}

namespace sa2
{

  void printAudioInfo()
  {
    for (const auto & it : activeSoundGenerators)
    {
      const auto & generator = it.second;
      generator->printInfo();
    }
  }

  void resetAudioUnderruns()
  {
    for (const auto & it : activeSoundGenerators)
    {
      const auto & generator = it.second;
      generator->ResetUnderruns();
    }
  }

  std::vector<SoundInfo> getAudioInfo()
  {
    std::vector<SoundInfo> info;
    info.reserve(activeSoundGenerators.size());

    for (const auto & it : activeSoundGenerators)
    {
      const auto & generator = it.second;
      info.push_back(generator->getInfo());
    }

    return info;
  }

  void setAudioOptions(const common2::EmulatorOptions & options)
  {
    audioDeviceName = options.audioDeviceName;
    audioBuffer = options.audioBuffer;
  }

}
