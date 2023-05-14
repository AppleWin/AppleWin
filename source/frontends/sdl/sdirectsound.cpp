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

  class DirectSoundGenerator
  {
  public:
    DirectSoundGenerator(IDirectSoundBuffer * buffer);
    ~DirectSoundGenerator();

    void stop();
    void writeAudio(const size_t ms);
    void resetUnderruns();

    void printInfo() const;
    sa2::SoundInfo getInfo() const;

  private:
    static void staticAudioCallback(void* userdata, Uint8* stream, int len);

    void audioCallback(Uint8* stream, int len);

    IDirectSoundBuffer * myBuffer;

    std::vector<Uint8> myMixerBuffer;

    SDL_AudioDeviceID myAudioDevice;
    SDL_AudioSpec myAudioSpec;

    size_t myBytesPerSecond;

    void close();
    bool isRunning() const;

    Uint8 * mixBufferTo(Uint8 * stream);
  };

  std::unordered_map<IDirectSoundBuffer *, std::shared_ptr<DirectSoundGenerator>> activeSoundGenerators;

  void DirectSoundGenerator::staticAudioCallback(void* userdata, Uint8* stream, int len)
  {
    DirectSoundGenerator * generator = static_cast<DirectSoundGenerator *>(userdata);
    return generator->audioCallback(stream, len);
  }

  void DirectSoundGenerator::audioCallback(Uint8* stream, int len)
  {
    LPVOID lpvAudioPtr1, lpvAudioPtr2;
    DWORD dwAudioBytes1, dwAudioBytes2;
    myBuffer->Read(len, &lpvAudioPtr1, &dwAudioBytes1, &lpvAudioPtr2, &dwAudioBytes2);

    const size_t bytesRead = dwAudioBytes1 + dwAudioBytes2;
    myMixerBuffer.resize(bytesRead);

    Uint8 * dest = myMixerBuffer.data();
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
      myBuffer->SetBufferUnderrun();
      memset(stream, myAudioSpec.silence, gap);
    }
  }

  DirectSoundGenerator::DirectSoundGenerator(IDirectSoundBuffer * buffer)
    : myBuffer(buffer)
    , myAudioDevice(0)
    , myBytesPerSecond(0)
  {
    SDL_zero(myAudioSpec);
  }

  DirectSoundGenerator::~DirectSoundGenerator()
  {
    close();
  }

  void DirectSoundGenerator::close()
  {
    SDL_CloseAudioDevice(myAudioDevice);
    myAudioDevice = 0;
  }

  bool DirectSoundGenerator::isRunning() const
  {
    return myAudioDevice;
  }

  void DirectSoundGenerator::printInfo() const
  {
    if (isRunning())
    {
      const DWORD bytesInBuffer = myBuffer->GetBytesInBuffer();
      std::cerr << "Channels: " << (int)myAudioSpec.channels;
      std::cerr << ", buffer: " << std::setw(6) << bytesInBuffer;
      const double time = double(bytesInBuffer) / myBytesPerSecond * 1000;
      std::cerr << ", " << std::setw(8) << time << " ms";
      std::cerr << ", underruns: " << std::setw(10) << myBuffer->GetBufferUnderruns() << std::endl;
    }
  }

  sa2::SoundInfo DirectSoundGenerator::getInfo() const
  {
    sa2::SoundInfo info;
    info.running = isRunning();
    info.channels = myAudioSpec.channels;
    info.volume = myBuffer->GetLogarithmicVolume();
    info.numberOfUnderruns = myBuffer->GetBufferUnderruns();

    if (info.running && myBytesPerSecond > 0)
    {
      const DWORD bytesInBuffer = myBuffer->GetBytesInBuffer();
      const float coeff = 1.0 / myBytesPerSecond;
      info.buffer = bytesInBuffer * coeff;
      info.size = myBuffer->bufferSize * coeff;
    }

    return info;
  }

  void DirectSoundGenerator::resetUnderruns()
  {
    myBuffer->ResetUnderrruns();
  }

  void DirectSoundGenerator::stop()
  {
    if (myAudioDevice)
    {
      SDL_PauseAudioDevice(myAudioDevice, 1);
      close();
    }
  }

  Uint8 * DirectSoundGenerator::mixBufferTo(Uint8 * stream)
  {
    // we could copy ADJUST_VOLUME from SDL_mixer.c and avoid all copying and (rare) race conditions
    const double logVolume = myBuffer->GetLogarithmicVolume();
    // same formula as QAudio::convertVolume()
    const double linVolume = logVolume > 0.99 ? 1.0 : -std::log(1.0 - logVolume) / std::log(100.0);
    const Uint8 svolume = Uint8(linVolume * SDL_MIX_MAXVOLUME);

    const size_t len = myMixerBuffer.size();
    memset(stream, 0, len);
    SDL_MixAudioFormat(stream, myMixerBuffer.data(), myAudioSpec.format, len, svolume);
    return stream + len;
  }

  void DirectSoundGenerator::writeAudio(const size_t ms)
  {
    // this is autostart as we only do for the palying buffers
    // and AW might activate one later
    if (myAudioDevice)
    {
      return;
    }

    DWORD dwStatus;
    myBuffer->GetStatus(&dwStatus);
    if (!(dwStatus & DSBSTATUS_PLAYING))
    {
      return;
    }

    SDL_AudioSpec want;
    SDL_zero(want);

    want.freq = myBuffer->sampleRate;
    want.format = AUDIO_S16LSB;
    want.channels = myBuffer->channels;
    want.samples = std::min<size_t>(MAX_SAMPLES, nextPowerOf2(myBuffer->sampleRate * ms / 1000));
    want.callback = staticAudioCallback;
    want.userdata = this;
    myAudioDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &myAudioSpec, 0);

    if (myAudioDevice)
    {
      myBytesPerSecond = getBytesPerSecond(myAudioSpec);

      SDL_PauseAudioDevice(myAudioDevice, 0);
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
    // stop the QAudioOutput before removing. is this necessary?
    it->second->stop();
    activeSoundGenerators.erase(it);
  }
}

namespace sa2
{

  void stopAudio()
  {
    for (auto & it : activeSoundGenerators)
    {
      const std::shared_ptr<DirectSoundGenerator> & generator = it.second;
      generator->stop();
    }
  }

  void writeAudio(const size_t ms)
  {
    for (auto & it : activeSoundGenerators)
    {
      const std::shared_ptr<DirectSoundGenerator> & generator = it.second;
      generator->writeAudio(ms);
    }
  }

  void printAudioInfo()
  {
    for (auto & it : activeSoundGenerators)
    {
      const std::shared_ptr<DirectSoundGenerator> & generator = it.second;
      generator->printInfo();
    }
  }

  void resetUnderruns()
  {
    for (auto & it : activeSoundGenerators)
    {
      const std::shared_ptr<DirectSoundGenerator> & generator = it.second;
      generator->resetUnderruns();
    }
  }

  std::vector<SoundInfo> getAudioInfo()
  {
    std::vector<SoundInfo> info;
    info.reserve(activeSoundGenerators.size());

    for (auto & it : activeSoundGenerators)
    {
      const std::shared_ptr<DirectSoundGenerator> & generator = it.second;
      info.push_back(generator->getInfo());
    }

    return info;
  }

}
