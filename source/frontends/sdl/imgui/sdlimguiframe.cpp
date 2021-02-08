#include "StdAfx.h"
#include "frontends/sdl/imgui/sdlimguiframe.h"

#include "frontends/sdl/utils.h"
#include "frontends/common2/fileregistry.h"
#include "frontends/sdl/imgui/image.h"
#include "frontends/sdl/imgui/settingshelper.h"

#include "Interface.h"
#include "Core.h"
#include "CPU.h"
#include "CardManager.h"
#include "Speaker.h"
#include "Mockingboard.h"
#include "Registry.h"

#include <iostream>

namespace
{

  void HelpMarker(const char* desc)
  {
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
      ImGui::BeginTooltip();
      ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
      ImGui::TextUnformatted(desc);
      ImGui::PopTextWrapPos();
      ImGui::EndTooltip();
    }
  }

}

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

  SetApplicationIcon();

  myGLContext = SDL_GL_CreateContext(myWindow.get());
  if (!myGLContext)
  {
    throw std::runtime_error(SDL_GetError());
  }

  SDL_GL_MakeCurrent(myWindow.get(), myGLContext);

  // Setup Platform/Renderer backends
  std::cerr << "IMGUI_VERSION: " << IMGUI_VERSION << std::endl;
  std::cerr << "GL_VENDOR: " << glGetString(GL_VENDOR) << std::endl;
  std::cerr << "GL_RENDERER: " << glGetString(GL_RENDERER) << std::endl;
  std::cerr << "GL_VERSION: " << glGetString(GL_VERSION) << std::endl;
  std::cerr << "GL_SHADING_LANGUAGE_VERSION: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
  //  const char* runtime_gl_extensions = (const char*)glGetString(GL_EXTENSIONS);
  //  std::cerr << "GL_EXTENSIONS: " << runtime_gl_extensions << std::endl;

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();

  mySettings.iniFileLocation = GetConfigFile("imgui.ini");
  if (mySettings.iniFileLocation.empty())
  {
    io.IniFilename = nullptr;
  }
  else
  {
    io.IniFilename = mySettings.iniFileLocation.c_str();
  }

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

  allocateTexture(myTexture, myBorderlessWidth, myBorderlessHeight);
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
  loadTextureFromData(myTexture, myFramebuffer.data() + myOffset, myBorderlessWidth, myBorderlessHeight, myPitch);
}

void SDLImGuiFrame::DrawAppleVideo()
{
  // need to flip the texture vertically
  const ImVec2 uv0(0, 1);
  const ImVec2 uv1(1, 0);

  ImGuiIO& io = ImGui::GetIO();

  if (mySettings.windowed)
  {
    if (ImGui::Begin("Apple ]["))
    {
      ImGui::Image(myTexture, ImGui::GetContentRegionAvail(), uv0, uv1);
    }
    ImGui::End();

    // must clean background if in windowed mode
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    const ImVec4 background(0.45f, 0.55f, 0.60f, 1.00f);
    glClearColor(background.x, background.y, background.z, background.w);
    glClear(GL_COLOR_BUFFER_BIT);
  }
  else
  {
    const ImVec2 zero(0, 0);
    // draw on the background
    ImGui::GetBackgroundDrawList()->AddImage(myTexture, zero, io.DisplaySize, uv0, uv1);
  }
}

void SDLImGuiFrame::ShowSettings()
{
  if (ImGui::Begin("Settings"))
  {
    ImGuiIO& io = ImGui::GetIO();

    if (ImGui::BeginTabBar("Settings"))
    {
      if (ImGui::BeginTabItem("General"))
      {
	ImGui::Checkbox("Apple Video windowed", &mySettings.windowed);
	ImGui::SameLine(); HelpMarker("Show Apple Video in a separate window.");

	ImGui::Checkbox("Show Demo", &mySettings.showDemo);
	ImGui::SameLine(); HelpMarker("Show Dear ImGui DemoWindow.");

	ImGui::Text("FPS: %d", int(io.Framerate));
	ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Hardware"))
      {
        if (ImGui::BeginTable("Cards", 2, ImGuiTableFlags_RowBg))
        {
	  CardManager & manager = GetCardMgr();
	  ImGui::TableSetupColumn("Slot");
	  ImGui::TableSetupColumn("Card");
	  ImGui::TableHeadersRow();

	  for (size_t slot = 0; slot < 8; ++slot)
	  {
	    ImGui::TableNextColumn();
	    ImGui::Selectable(std::to_string(slot).c_str());
	    ImGui::TableNextColumn();
	    const SS_CARDTYPE card = manager.QuerySlot(slot);;
	    ImGui::Selectable(getCardName(card).c_str());
	  }
	  ImGui::TableNextColumn();
	  ImGui::Selectable("AUX");
	  ImGui::TableNextColumn();
	  const SS_CARDTYPE card = manager.QueryAux();
	  ImGui::Selectable(getCardName(card).c_str());

	  ImGui::EndTable();
        }
	ImGui::Separator();

        if (ImGui::BeginTable("Type", 2, ImGuiTableFlags_RowBg))
        {
	  ImGui::TableNextColumn();
	  ImGui::Selectable("Apple 2");
	  ImGui::TableNextColumn();
	  const eApple2Type a2e = GetApple2Type();
	  ImGui::Selectable(getApple2Name(a2e).c_str());

	  ImGui::TableNextColumn();
	  ImGui::Selectable("CPU");
	  ImGui::TableNextColumn();
	  const eCpuType cpu = GetMainCpu();
	  ImGui::Selectable(getCPUName(cpu).c_str());

	  ImGui::EndTable();
        }

	ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Audio"))
      {
	const int volumeMax = GetPropertySheet().GetVolumeMax();
	if (ImGui::SliderInt("Speaker volume", &mySettings.speakerVolume, 0, volumeMax))
	{
	  SpkrSetVolume(volumeMax - mySettings.speakerVolume, volumeMax);
	  REGSAVE(TEXT(REGVALUE_SPKR_VOLUME), SpkrGetVolume());
	}

	if (ImGui::SliderInt("Mockingboard volume", &mySettings.mockingboardVolume, 0, volumeMax))
	{
	  MB_SetVolume(volumeMax - mySettings.mockingboardVolume, volumeMax);
	  REGSAVE(TEXT(REGVALUE_MB_VOLUME), MB_GetVolume());
	}

	ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }

  }
  ImGui::End();
}

void SDLImGuiFrame::RenderPresent()
{
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame(myWindow.get());
  ImGui::NewFrame();

  ShowSettings();

  if (mySettings.showDemo)
  {
    ImGui::ShowDemoWindow();
  }

  DrawAppleVideo();

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  SDL_GL_SwapWindow(myWindow.get());
}
