#include "frontends/sdl/sdirectsound.h"

#include "windows.h"
#include "linux/linuxinterface.h"

#include <SDL.h>

#include <unordered_map>
#include <memory>
#include <iostream>
#include <iomanip>

namespace
{

  class DirectSoundGenerator
  {
  public:
    DirectSoundGenerator(IDirectSoundBuffer * buffer);
    ~DirectSoundGenerator();

    void stop();
    void writeAudio();

    void printInfo() const;
    sa2::SoundInfo getInfo() const;

  private:
    IDirectSoundBuffer * myBuffer;

    std::vector<Uint8> myMixerBuffer;

    SDL_AudioDeviceID myAudioDevice;
    SDL_AudioSpec myAudioSpec;

    int myBytesPerSecond;

    void close();
    bool isRunning() const;
    bool isRunning();

    void mixBuffer(const void * ptr, const size_t size);
  };


  std::unordered_map<IDirectSoundBuffer *, std::shared_ptr<DirectSoundGenerator>> activeSoundGenerators;

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

  bool DirectSoundGenerator::isRunning()
  {
    if (myAudioDevice)
    {
      return true;
    }

    DWORD dwStatus;
    myBuffer->GetStatus(&dwStatus);
    if (!(dwStatus & DSBSTATUS_PLAYING))
    {
      return false;
    }

    SDL_AudioSpec want;
    SDL_zero(want);

    want.freq = myBuffer->sampleRate;
    want.format = AUDIO_S16LSB;
    want.channels = myBuffer->channels;
    want.samples = 4096;  // what does this really mean?
    want.callback = nullptr;
    myAudioDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &myAudioSpec, 0);

    if (myAudioDevice)
    {
      const int bitsPerSample = myAudioSpec.format & SDL_AUDIO_MASK_BITSIZE;
      myBytesPerSecond = myAudioSpec.freq * myAudioSpec.channels * bitsPerSample / 8;

      SDL_PauseAudioDevice(myAudioDevice, 0);
      return true;
    }

    return false;
  }

  void DirectSoundGenerator::printInfo() const
  {
    if (isRunning())
    {
      const int width = 5;
      const DWORD bytesInBuffer = myBuffer->GetBytesInBuffer();
      const Uint32 bytesInQueue = SDL_GetQueuedAudioSize(myAudioDevice);
      std::cerr << "Channels: " << (int)myAudioSpec.channels;
      std::cerr << ", buffer: " << std::setw(width) << bytesInBuffer;
      std::cerr << ", SDL: " << std::setw(width) << bytesInQueue;
      const double queue = double(bytesInBuffer + bytesInQueue) / myBytesPerSecond;
      std::cerr << ", queue: " << queue << " s" << std::endl;
    }
  }

  sa2::SoundInfo DirectSoundGenerator::getInfo() const
  {
    sa2::SoundInfo info;
    info.running = isRunning();
    info.channels = myAudioSpec.channels;
    info.volume = myBuffer->GetLogarithmicVolume();

    if (info.running && myBytesPerSecond > 0.0)
    {
      const DWORD bytesInBuffer = myBuffer->GetBytesInBuffer();
      const Uint32 bytesInQueue = SDL_GetQueuedAudioSize(myAudioDevice);
      const float coeff = 1.0 / myBytesPerSecond;
      info.buffer = bytesInBuffer * coeff;
      info.queue = bytesInQueue * coeff;
      info.size = myBuffer->bufferSize * coeff;
    }

    return info;
  }

  void DirectSoundGenerator::stop()
  {
    if (myAudioDevice)
    {
      SDL_PauseAudioDevice(myAudioDevice, 1);
      close();
    }
  }

  void DirectSoundGenerator::mixBuffer(const void * ptr, const size_t size)
  {
    const double logVolume = myBuffer->GetLogarithmicVolume();
    // same formula as QAudio::convertVolume()
    const double linVolume = logVolume > 0.99 ? 1.0 : -std::log(1.0 - logVolume) / std::log(100.0);

    const Uint8 svolume = Uint8(linVolume * SDL_MIX_MAXVOLUME);

    // this is a bit of a waste copy-time, but it reuses SDL to do it properly
    myMixerBuffer.resize(size);
    memset(myMixerBuffer.data(), 0, size);
    SDL_MixAudioFormat(myMixerBuffer.data(), (const Uint8*)ptr, myAudioSpec.format, size, svolume);
    SDL_QueueAudio(myAudioDevice, myMixerBuffer.data(), size);
  }

  void DirectSoundGenerator::writeAudio()
  {
    // this is autostart as we only do for the palying buffers
    // and AW might activate one later
    if (!isRunning())
    {
      return;
    }

    // SDL does not have a maximum buffer size
    // we have to be careful not writing too much
    // otherwise AW starts generating a lot of samples
    // and we loose sync

    // assume SDL's buffer is the same size as AW's
    const int bytesFree = myBuffer->bufferSize - SDL_GetQueuedAudioSize(myAudioDevice);

    if (bytesFree > 0)
    {
      LPVOID lpvAudioPtr1, lpvAudioPtr2;
      DWORD dwAudioBytes1, dwAudioBytes2;
      myBuffer->Read(bytesFree, &lpvAudioPtr1, &dwAudioBytes1, &lpvAudioPtr2, &dwAudioBytes2);

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

  void writeAudio()
  {
    for (auto & it : activeSoundGenerators)
    {
      const std::shared_ptr<DirectSoundGenerator> & generator = it.second;
      generator->writeAudio();
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
