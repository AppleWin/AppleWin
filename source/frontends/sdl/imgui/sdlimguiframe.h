#pragma once

#include "frontends/sdl/sdlframe.h"
#include "frontends/sdl/imgui/gles.h"

namespace common2
{
  class EmulatorOptions;
}

class SDLImGuiFrame : public SDLFrame
{
public:
  SDLImGuiFrame(const common2::EmulatorOptions & options);

  ~SDLImGuiFrame() override;

  void UpdateTexture() override;
  void RenderPresent() override;

protected:

  void ProcessSingleEvent(const SDL_Event & event, bool & quit) override;

private:

  void ClearBackground();
  void DrawAppleVideo();
  void ShowSettings();

  struct ImGuiSettings
  {
    std::string iniFileLocation;
    bool windowed = false;
    bool showDemo = false;
    bool showSettings = false;
    int speakerVolume = 50;
    int mockingboardVolume = 50;
  };

  size_t myPitch;
  size_t myOffset;
  size_t myBorderlessWidth;
  size_t myBorderlessHeight;

  SDL_GLContext myGLContext;
  ImTextureID myTexture;

  ImGuiSettings mySettings;
};
