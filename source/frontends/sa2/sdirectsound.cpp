#include "frontends/sa2/sdirectsound.h"

#include "linux/windows/dsound.h"

#include <unordered_map>
#include <memory>

namespace
{

  class DirectSoundGenerator
  {
  public:
    DirectSoundGenerator(IDirectSoundBuffer * buffer);
    ~DirectSoundGenerator();

    void stop();
    void writeAudio();

    void setOptions(const int initialSilence);

  private:
    IDirectSoundBuffer * myBuffer;

    SDL_AudioDeviceID myAudioDevice;
    SDL_AudioSpec myAudioSpec;

    // options
    int myInitialSilence;

    void close();
    void setVolume();
    bool isRunning();
    void writeEnoughSilence(const int ms);
  };


  std::unordered_map<IDirectSoundBuffer *, std::shared_ptr<DirectSoundGenerator>> activeSoundGenerators;

  DirectSoundGenerator::DirectSoundGenerator(IDirectSoundBuffer * buffer)
    : myBuffer(buffer)
    , myAudioDevice(0)
  {
    myInitialSilence = 200;
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

    SDL_memset(&want, 0, sizeof(want));
    want.freq = myBuffer->sampleRate;
    want.format = AUDIO_S16LSB;
    want.channels = myBuffer->channels;
    want.samples = 4096;  // what does this really mean?
    want.callback = nullptr;
    myAudioDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &myAudioSpec, 0);

    if (myAudioDevice)
    {
      SDL_PauseAudioDevice(myAudioDevice, 0);
      // we give ourselves a bit of time in case things are not smooth
      writeEnoughSilence(myInitialSilence);
      return true;
    }

    return false;
  }

  void DirectSoundGenerator::setOptions(const int initialSilence)
  {
    myInitialSilence = std::max(0, initialSilence);
  }

  void DirectSoundGenerator::setVolume()
  {
    LONG dwVolume = 0;
    myBuffer->GetVolume(&dwVolume);
    const double volume = - double(dwVolume) / DSBVOLUME_MIN + 1.0;
    // myAudioOutput->setVolume(volume);
  }

  void DirectSoundGenerator::stop()
  {
    if (myAudioDevice)
    {
      writeEnoughSilence(10);  // just ti ensure we end up in silence
      SDL_PauseAudioDevice(myAudioDevice, 1);
      close();
    }
  }

  void DirectSoundGenerator::writeEnoughSilence(const int ms)
  {
    // write a few ms of silence
    const int bitsPerSample = myAudioSpec.format & SDL_AUDIO_MASK_BITSIZE;
    const int framesToWrite = (myAudioSpec.freq * ms * myAudioSpec.channels) / 1000;  // target frames to write
    const int bytesToWrite = framesToWrite * bitsPerSample / 8;

    std::vector<char> silence(bytesToWrite);
    SDL_QueueAudio(myAudioDevice, silence.data(), silence.size());
  }

  void DirectSoundGenerator::writeAudio()
  {
    // this is autostart as we only do for the palying buffers
    // and AW might activate one later
    if (!isRunning())
    {
      return;
    }

    // this is woraround of the AW nErrorInc calculation
    // we are basically reading data too fast
    // and AW then thinks we are running behind, and produces more data
    // shoudl we instead check how much data is queued in SDL using SDL_GetQueuedAudioSize()?

    const int availableBytes = myBuffer->GetAvailableBytes();
    const int bufferSize = myBuffer->bufferSize;
    const int minimumAvailable = bufferSize / 4;  // look in Speaker.cpp and Mockingboard.cpp

    if (availableBytes > minimumAvailable)
    {
      const int toRead = availableBytes - minimumAvailable;

      LPVOID lpvAudioPtr1, lpvAudioPtr2;
      DWORD dwAudioBytes1, dwAudioBytes2;
      myBuffer->Read(toRead, &lpvAudioPtr1, &dwAudioBytes1, &lpvAudioPtr2, &dwAudioBytes2);

      if (lpvAudioPtr1 && dwAudioBytes1)
      {
	SDL_QueueAudio(myAudioDevice, lpvAudioPtr1, dwAudioBytes1);
      }
      if (lpvAudioPtr2 && dwAudioBytes2)
      {
	SDL_QueueAudio(myAudioDevice, lpvAudioPtr2, dwAudioBytes2);
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

namespace SDirectSound
{

  void stop()
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

  void setOptions(const int initialSilence)
  {
    for (auto & it : activeSoundGenerators)
    {
      const std::shared_ptr<DirectSoundGenerator> & generator = it.second;
      generator->setOptions(initialSilence);
    }
  }

}
