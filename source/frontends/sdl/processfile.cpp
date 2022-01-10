#include "StdAfx.h"
#include "frontends/sdl/processfile.h"
#include "frontends/sdl/sdlframe.h"
#include "frontends/common2/utils.h"

#include "linux/tape.h"

#include "CardManager.h"
#include "Disk.h"
#include "Core.h"
#include "Harddisk.h"

namespace
{

  void insertDisk(sa2::SDLFrame * frame, const char * filename, const size_t dragAndDropSlot, const size_t dragAndDropDrive)
  {
    CardManager & cardManager = GetCardMgr();
    const SS_CARDTYPE cardInSlot = cardManager.QuerySlot(dragAndDropSlot);
    switch (cardInSlot)
    {
      case CT_Disk2:
      {
        Disk2InterfaceCard * card2 = dynamic_cast<Disk2InterfaceCard*>(cardManager.GetObj(dragAndDropSlot));
        const ImageError_e error = card2->InsertDisk(dragAndDropDrive, filename, IMAGE_USE_FILES_WRITE_PROTECT_STATUS, IMAGE_DONT_CREATE);
        if (error != eIMAGE_ERROR_NONE)
        {
          card2->NotifyInvalidImage(dragAndDropDrive, filename, error);
        }
        break;
      }
      case CT_GenericHDD:
      {
        HarddiskInterfaceCard * harddiskCard = dynamic_cast<HarddiskInterfaceCard*>(cardManager.GetObj(dragAndDropSlot));
        if (!harddiskCard->Insert(dragAndDropDrive, filename))
        {
          frame->FrameMessageBox("Invalid HD image", "ERROR", MB_OK);
        }
        break;
      }
      default:
      {
        frame->FrameMessageBox("Invalid D&D target", "ERROR", MB_OK);
      }
    }
  }

  void insertTape(sa2::SDLFrame * frame, const char * filename)
  {
    SDL_AudioSpec wavSpec;
    Uint32 wavLength;
    Uint8 *wavBuffer;
    if (!SDL_LoadWAV(filename, &wavSpec, &wavBuffer, &wavLength))
    {
      frame->FrameMessageBox("Could not open wav file", "ERROR", MB_OK);
      return;
    }

    SDL_AudioCVT cvt;
    // tested with all formats from https://asciiexpress.net/
    // 8 bit mono is just enough
    // TAPEIN will interpolate so we do not need to resample at a higher frequency
    const SDL_AudioFormat format = sizeof(CassetteTape::tape_data_t) == 1 ? AUDIO_S8 : AUDIO_S16SYS;
    const int res = SDL_BuildAudioCVT(&cvt, wavSpec.format, wavSpec.channels, wavSpec.freq, format, 1, wavSpec.freq);
    cvt.len = wavLength;
    std::vector<CassetteTape::tape_data_t> output(cvt.len_mult * cvt.len / sizeof(CassetteTape::tape_data_t));
    std::memcpy(output.data(), wavBuffer, cvt.len);
    SDL_FreeWAV(wavBuffer);

    if (res)
    {
      cvt.buf = reinterpret_cast<Uint8 *>(output.data());
      SDL_ConvertAudio(&cvt);
      output.resize(cvt.len_cvt / sizeof(CassetteTape::tape_data_t));
    }

    CassetteTape::instance().setData(filename, output, wavSpec.freq);
  }

}


namespace sa2
{

  void processFile(SDLFrame * frame, const char * filename, const size_t dragAndDropSlot, const size_t dragAndDropDrive)
  {
    const char * yaml = ".yaml";
    const char * wav = ".wav";
    if (strlen(filename) > strlen(yaml) && !strcmp(filename + strlen(filename) - strlen(yaml), yaml))
    {
      common2::setSnapshotFilename(filename);
      frame->LoadSnapshot();
    }
    else if (strlen(filename) > strlen(wav) && !strcmp(filename + strlen(filename) - strlen(wav), wav))
    {
      insertTape(frame, filename);
    }
    else
    {
      insertDisk(frame, filename, dragAndDropSlot, dragAndDropDrive);
    }
    frame->ResetSpeed();
  }
  
}
