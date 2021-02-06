#include "StdAfx.h"
#include "frontends/sdl/imgui/sdlimguiframe.h"
#include "frontends/sdl/imgui/image.h"
#include "frontends/sdl/utils.h"

#include "Interface.h"
#include "Core.h"

#include <iostream>

SDLImGuiFrame::SDLImGuiFrame()
{
  Video & video = GetVideo();

  myBorderlessWidth = video.GetFrameBufferBorderlessWidth();
  myBorderlessHeight = video.GetFrameBufferBorderlessHeight();

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, SDL_CONTEXT_MAJOR); // from local gles.h
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

  // Create window with graphics context
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  SDL_WindowFlags windowFlags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  myWindow.reset(SDL_CreateWindow(g_pAppTitle.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, myBorderlessWidth, myBorderlessHeight, windowFlags), SDL_DestroyWindow);
  if (!myWindow)
  {
    throw std::runtime_error(SDL_GetError());
  }

  myGLContext = SDL_GL_CreateContext(myWindow.get());
  if (!myGLContext)
  {
    throw std::runtime_error(SDL_GetError());
  }

  SDL_GL_MakeCurrent(myWindow.get(), myGLContext);

  // Setup Platform/Renderer backends
  std::cerr << "GL_VENDOR: " << glGetString(GL_VENDOR) << std::endl;
  std::cerr << "GL_RENDERER: " << glGetString(GL_RENDERER) << std::endl;
  std::cerr << "GL_VERSION: " << glGetString(GL_VERSION) << std::endl;
  std::cerr << "GL_SHADING_LANGUAGE_VERSION: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
  //  const char* runtime_gl_extensions = (const char*)glGetString(GL_EXTENSIONS);
  //  std::cerr << "GL_EXTENSIONS: " << runtime_gl_extensions << std::endl;

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

  ImGui::StyleColorsDark();

  ImGui_ImplSDL2_InitForOpenGL(myWindow.get(), myGLContext);

  ImGui_ImplOpenGL3_Init();

  glGenTextures(1, &myTexture);

  const int width = video.GetFrameBufferWidth();
  const size_t borderWidth = video.GetFrameBufferBorderWidth();
  const size_t borderHeight = video.GetFrameBufferBorderHeight();

  myPitch = width;
  myOffset = (width * borderHeight + borderWidth) * sizeof(bgra_t);
}

SDLImGuiFrame::~SDLImGuiFrame()
{
  glDeleteTextures(1, &myTexture);
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  SDL_GL_DeleteContext(myGLContext);
}

void SDLImGuiFrame::UpdateTexture()
{
  LoadTextureFromData(myTexture, myFramebuffer.data() + myOffset, myBorderlessWidth, myBorderlessHeight, myPitch);
}

void SDLImGuiFrame::RenderPresent()
{
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame(myWindow.get());
  ImGui::NewFrame();

  // need to flip the texture vertically
  const ImVec2 uv0(0, 1);
  const ImVec2 uv1(1, 0);
  const ImVec2 zero(0, 0);
  // draw on the background
  ImGui::GetBackgroundDrawList()->AddImage(myTexture, zero, ImGui::GetIO().DisplaySize, uv0, uv1);

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  SDL_GL_SwapWindow(myWindow.get());
}
