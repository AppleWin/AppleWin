#pragma once

#include <SDL.h>
#include <memory>

class Emulator
{
public:
  Emulator(
    const std::shared_ptr<SDL_Window> & window,
    const std::shared_ptr<SDL_Renderer> & renderer,
    const std::shared_ptr<SDL_Texture> & texture
    );

  void executeOneFrame();
  void refreshVideo();
  SDL_Rect updateTexture();
  void refreshVideo(const SDL_Rect & rect);

  void processEvents(bool & quit);

private:
  void processKeyDown(const SDL_KeyboardEvent & key, bool & quit);
  void processKeyUp(const SDL_KeyboardEvent & key);
  void processText(const SDL_TextInputEvent & text);

  const std::shared_ptr<SDL_Window> myWindow;
  const std::shared_ptr<SDL_Renderer> myRenderer;
  const std::shared_ptr<SDL_Texture> myTexture;
  const int myFPS;

  int myMultiplier;
  bool myFullscreen;
  int myExtraCycles;
};
