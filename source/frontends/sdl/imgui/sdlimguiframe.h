#pragma once

#include "frontends/sdl/sdlframe.h"
#include "frontends/sdl/imgui/sdlsettings.h"
#include "frontends/sdl/imgui/gles.h"

namespace common2
{
  struct EmulatorOptions;
}

namespace sa2
{

  class SDLImGuiFrame : public SDLFrame
  {
  public:
    SDLImGuiFrame(const common2::EmulatorOptions & options);

    ~SDLImGuiFrame() override;

    void UpdateTexture() override;
    void RenderPresent() override;

    void ResetSpeed() override;

  protected:

    void ProcessSingleEvent(const SDL_Event & event, bool & quit) override;

  private:

    void ClearBackground();
    void DrawAppleVideo();

    size_t myPitch;
    size_t myOffset;
    size_t myBorderlessWidth;
    size_t myBorderlessHeight;

    SDL_GLContext myGLContext;
    ImTextureID myTexture;

    std::string myIniFileLocation;
    ImGuiSettings mySettings;
  };

}
