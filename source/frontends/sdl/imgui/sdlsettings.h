#pragma once

#include "frontends/sdl/imgui/glselector.h"
#include "frontends/sdl/imgui/sdldebugger.h"
#include "frontends/sdl/imgui/sdlmemory.h"
#include "frontends/sdl/imgui/imgui-filebrowser/imfilebrowser.h"
#include "frontends/sdl/sdirectsound.h"

namespace sa2
{

    class SDLFrame;

    class ImGuiSettings
    {
    public:
        ImGuiSettings();

        void show(SDLFrame *frame, ImFont *debuggerFont);
        float drawMenuBar(SDLFrame *frame, const bool enabled);
        void resetDebuggerCycles();
        void showDiskTab();
        void toggleSettings();
        void toggleShortcuts();

        bool windowed = false;
        bool quit = false;

    private:
        bool myShowDemo = false;
        bool myShowSettings = false;
        bool myShowMemoryEditor = false;
        bool myShowAbout = false;
        bool myShowShortcuts = false;
        bool myShowDiskTab = false;

        int mySpeakerVolume;
        int myMockingboardVolume;

        size_t myOpenSlot = 0;
        size_t myOpenDrive = 0;

        ImGuiDebugger myDebugger;
        ImGuiMemory myMemoryViewer;
        ImGui::FileBrowser myDiskFileDialog;
        ImGui::FileBrowser mySaveFileDialog;

        std::vector<MemoryEditor> myMemoryEditors;

        std::vector<SoundInfo> myAudioInfo;

        void showSettings(SDLFrame *frame);
        void showMemoryEditor();
        void showAboutWindow();
        void showShortcutWindow();
        void openFileDialog(ImGui::FileBrowser &browser, const std::string &filename);
        void openDiskFileDialog(
            ImGui::FileBrowser &browser, const std::string &diskName, const size_t slot, const size_t drive);
    };

} // namespace sa2
