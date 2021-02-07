#pragma once

#include "frontends/sdl/sdlframe.h"
#include "frontends/sdl/imgui/gles.h"

class SDLImGuiFrame : public SDLFrame
{
public:
  SDLImGuiFrame();

  ~SDLImGuiFrame() override;

  void UpdateTexture() override;
  void RenderPresent() override;

private:

  void DrawAppleVideo();
  void ShowSettings();

  struct ImGuiSettings
  {
    std::string iniFileLocation;
    bool windowed = false;
    bool showDemo = false;
  };

  size_t myPitch;
  size_t myOffset;
  size_t myBorderlessWidth;
  size_t myBorderlessHeight;

  SDL_GLContext myGLContext;
  ImTextureID myTexture;

  ImGuiSettings mySettings;
};
