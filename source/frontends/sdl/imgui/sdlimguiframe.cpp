#include "StdAfx.h"
#include "frontends/sdl/imgui/sdlimguiframe.h"

#include "frontends/sdl/utils.h"
#include "frontends/common2/fileregistry.h"
#include "frontends/common2/programoptions.h"
#include "frontends/sdl/imgui/image.h"

#include "Interface.h"
#include "Core.h"

#include <iostream>


namespace
{

  std::string safeGlGetString(GLenum name)
  {
    const char * str = reinterpret_cast<const char *>(glGetString(name));
    if (str)
    {
      return str;
    }
    else
    {
      return "<MISSING>";
    }
  }

}


namespace sa2
{

  SDLImGuiFrame::SDLImGuiFrame(const common2::EmulatorOptions & options)
    : SDLFrame(options)
    , myPresenting(false)
  {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SA2_CONTEXT_FLAGS);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SA2_CONTEXT_PROFILE_MASK);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, SA2_CONTEXT_MAJOR_VERSION); // from local gles.h
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, SA2_CONTEXT_MINOR_VERSION);

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    const SDL_WindowFlags windowFlags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    const common2::Geometry & geometry = options.geometry;

    myWindow.reset(SDL_CreateWindow(g_pAppTitle.c_str(), geometry.x, geometry.y, geometry.width, geometry.height, windowFlags), SDL_DestroyWindow);
    if (!myWindow)
    {
      throw std::runtime_error(decorateSDLError("SDL_CreateWindow"));
    }

    SetApplicationIcon();

    myGLContext = SDL_GL_CreateContext(myWindow.get());
    if (!myGLContext)
    {
      throw std::runtime_error(decorateSDLError("SDL_GL_CreateContext"));
    }

    SDL_GL_MakeCurrent(myWindow.get(), myGLContext);

    // Setup Platform/Renderer backends
    std::cerr << "IMGUI_VERSION: " << IMGUI_VERSION << std::endl;
    std::cerr << "GL_VENDOR: " << safeGlGetString(GL_VENDOR) << std::endl;
    std::cerr << "GL_RENDERER: " << safeGlGetString(GL_RENDERER) << std::endl;
    std::cerr << "GL_VERSION: " << safeGlGetString(GL_VERSION) << std::endl;
    std::cerr << "GL_SHADING_LANGUAGE_VERSION: " << safeGlGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    //  const char* runtime_gl_extensions = (const char*)safeGlGetString(GL_EXTENSIONS);
    //  std::cerr << "GL_EXTENSIONS: " << runtime_gl_extensions << std::endl;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    myIniFileLocation = common2::GetConfigFile("imgui.ini");
    if (myIniFileLocation.empty())
    {
      io.IniFilename = nullptr;
    }
    else
    {
      io.IniFilename = myIniFileLocation.c_str();
    }

    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(myWindow.get(), myGLContext);
    ImGui_ImplOpenGL3_Init();

    myDeadTopZone = 0;
    myTexture = 0;
  }

  SDLImGuiFrame::~SDLImGuiFrame()
  {
    glDeleteTextures(1, &myTexture);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(myGLContext);
  }

  void SDLImGuiFrame::Initialize(bool resetVideoState)
  {
    SDLFrame::Initialize(resetVideoState);
    glDeleteTextures(1, &myTexture);
    glGenTextures(1, &myTexture);

    Video & video = GetVideo();

    myBorderlessWidth = video.GetFrameBufferBorderlessWidth();
    myBorderlessHeight = video.GetFrameBufferBorderlessHeight();

    const int width = video.GetFrameBufferWidth();
    const size_t borderWidth = video.GetFrameBufferBorderWidth();
    const size_t borderHeight = video.GetFrameBufferBorderHeight();

    myPitch = width;
    myOffset = (width * borderHeight + borderWidth) * sizeof(bgra_t);

    allocateTexture(myTexture, myBorderlessWidth, myBorderlessHeight);
  }

  void SDLImGuiFrame::UpdateTexture()
  {
    loadTextureFromData(myTexture, myFramebuffer.data() + myOffset, myBorderlessWidth, myBorderlessHeight, myPitch);
  }

  void SDLImGuiFrame::ClearBackground()
  {
    const ImVec4 background(0.45f, 0.55f, 0.60f, 1.00f);
    glClearColor(background.x, background.y, background.z, background.w);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  void SDLImGuiFrame::DrawAppleVideo()
  {
    // need to flip the texture vertically
    const ImVec2 uv0(0, 1);
    const ImVec2 uv1(1, 0);

    const float menuBarHeight = mySettings.drawMenuBar(this);
    myDeadTopZone = menuBarHeight;

    if (mySettings.windowed)
    {
      if (ImGui::Begin("Apple ]["))
      {
        UpdateTexture();
        ImGui::Image(myTexture, ImGui::GetContentRegionAvail(), uv0, uv1);
      }
      ImGui::End();
    }
    else
    {
      UpdateTexture();
      const ImVec2 zero(0, menuBarHeight);
      // draw on the background
      ImGuiIO& io = ImGui::GetIO();
      ImGui::GetBackgroundDrawList()->AddImage(myTexture, zero, io.DisplaySize, uv0, uv1);
    }
  }

  void SDLImGuiFrame::GetRelativeMousePosition(const SDL_MouseMotionEvent & motion, double & x, double & y) const
  {
    // this currently ignores a windowed apple video and applies to the whole sdl window
    int width, height;
    SDL_GetWindowSize(myWindow.get(), &width, &height);

    const int posY = std::max(motion.y - myDeadTopZone, 0);
    height = std::max(height - myDeadTopZone, 1);  // a real window has a minimum size of 1

    x = GetRelativePosition(motion.x, width);
    y = GetRelativePosition(posY, height);
  }

  void SDLImGuiFrame::VideoPresentScreen()
  {
    // this is NOT REENTRANT
    // the debugger (executed via mySettings.show(this)) might call it recursively
    if (!myPresenting)
    {
      myPresenting = true;
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplSDL2_NewFrame();
      ImGui::NewFrame();

      // "this" is a bit circular
      mySettings.show(this);
      DrawAppleVideo();

      ImGui::Render();
      ClearBackground();
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
      SDL_GL_SwapWindow(myWindow.get());
      myPresenting = false;
    }
  }

  void SDLImGuiFrame::ProcessSingleEvent(const SDL_Event & event, bool & quit)
  {
    ImGui_ImplSDL2_ProcessEvent(&event);

    switch (event.type)
    {
    case SDL_KEYDOWN:
    case SDL_KEYUP:
    case SDL_TEXTINPUT:
      {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureKeyboard)
        {
          return; // do not pass on
        }
      }
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEMOTION:
      {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse)
        {
          return; // do not pass on
        }
      }
    }

    SDLFrame::ProcessSingleEvent(event, quit);
  }

  void SDLImGuiFrame::ResetSpeed()
  {
    SDLFrame::ResetSpeed();
    mySettings.resetDebuggerCycles();
  }

}
