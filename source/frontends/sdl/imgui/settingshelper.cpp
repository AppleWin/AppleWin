#include "StdAfx.h"
#include "CardManager.h"
#include "Registry.h"
#include "Core.h"
#include "Memory.h"
#include "Interface.h"
#include "Debugger/Debug.h"

#include "frontends/sdl/imgui/settingshelper.h"

#include "frontends/sdl/imgui/glselector.h"
#include "imgui_internal.h"

#include <SDL.h>

// not very orthodox
void CreateLanguageCard(void);

namespace
{
    const std::map<SS_CARDTYPE, std::string> cards = {
        {CT_Empty, "Empty"},
        {CT_Disk2, "Disk II"},
        {CT_SSC, "SSC"},
        {CT_MockingboardC, "Mockingboard"},
        {CT_MegaAudio, "MEGA Audio"},
        {CT_SDMusic, "SD Music"},
        {CT_GenericPrinter, "Generic Printer"},
        {CT_GenericHDD, "Generic HDD"},
        {CT_GenericClock, "Generic Clock"},
        {CT_MouseInterface, "Mouse Interface"},
        {CT_Z80, "Z80"},
        {CT_Phasor, "Phasor"},
        {CT_Echo, "Echo"},
        {CT_SAM, "SAM"},
        {CT_80Col, "80 Column"},
        {CT_Extended80Col, "Extended 80 Column"},
        {CT_RamWorksIII, "RamWorks III"},
        {CT_Uthernet, "Uthernet"},
        {CT_LanguageCard, "Language Card"},
        {CT_LanguageCardIIe, "Language Card IIe"},
        {CT_Saturn128K, "Saturn 128K"},
        {CT_FourPlay, "4Play"},
        {CT_SNESMAX, "SNES MAX"},
        {CT_VidHD, "VidHD"},
        {CT_Uthernet2, "Uthernet //"},
    };

    const std::map<eApple2Type, std::string> apple2Types = {
        {A2TYPE_APPLE2, "Apple ][ (Original)"},
        {A2TYPE_APPLE2PLUS, "Apple ][+"},
        {A2TYPE_APPLE2JPLUS, "Apple ][ J-Plus"},
        {A2TYPE_APPLE2E, "Apple //e"},
        {A2TYPE_APPLE2EENHANCED, "Enhanced Apple //e"},
        {A2TYPE_APPLE2C, "Apple IIc"},
        {A2TYPE_PRAVETS8M, "Pravets 8M"},
        {A2TYPE_PRAVETS82, "Pravets 82"},
        {A2TYPE_BASE64A, "Base 64A"},
        {A2TYPE_PRAVETS8A, "Pravets 8A"},
        {A2TYPE_TK30002E, "TK3000 //e"},
    };

    const std::map<eCpuType, std::string> cpuTypes = {
        {CPU_6502, "6502"},
        {CPU_65C02, "65C02"},
        {CPU_Z80, "Z80"},
    };

    const std::map<AppMode_e, std::string> appModes = {
        {MODE_LOGO, "LOGO"},   {MODE_PAUSED, "PAUSED"},     {MODE_RUNNING, "RUNNING"},
        {MODE_DEBUG, "DEBUG"}, {MODE_STEPPING, "STEPPING"}, {MODE_BENCHMARK, "BENCHMARCK"},
    };

    const std::map<Disk_Status_e, std::string> statuses = {
        {DISK_STATUS_OFF, "OFF"},
        {DISK_STATUS_READ, "READ"},
        {DISK_STATUS_WRITE, "WRITE"},
        {DISK_STATUS_PROT, "PROT"},
    };

    const std::map<VideoType_e, std::string> videoTypes = {
        {VT_MONO_CUSTOM, "Monochrome (Custom)"},
        {VT_COLOR_IDEALIZED, "Color (Composite Idealized)"},
        {VT_COLOR_VIDEOCARD_RGB, "Color (RGB Card/Monitor)"},
        {VT_COLOR_MONITOR_NTSC, "Color (Composite Monitor)"},
        {VT_COLOR_TV, "Color TV"},
        {VT_MONO_TV, "B&W TV"},
        {VT_MONO_AMBER, "Monochrome (Amber)"},
        {VT_MONO_GREEN, "Monochrome (Green)"},
        {VT_MONO_WHITE, "Monochrome (White)"},
    };

    const std::map<DONGLETYPE, std::string> dongleTypes = {
        {DT_EMPTY, "Empty"},
        {DT_SDSSPEEDSTAR, "SDS DataKey - SpeedStar"},
        {DT_CODEWRITER, "Cortechs Corp - CodeWriter"},
        {DT_ROBOCOM500, "Robocom Ltd - Robo 500"},
        {DT_ROBOCOM1000, "Robocom Ltd - Robo 1000"},
        {DT_ROBOCOM1500, "Robocom Ltd - Robo 1500, CAD-2P"},
        {DT_HAYDENCOMPILER, "Hayden - Applesoft Compiler"},
    };

    const std::map<size_t, std::vector<SS_CARDTYPE>> cardsForSlots = {
        {0, {CT_Empty, CT_LanguageCard, CT_Saturn128K}},
        {1, {CT_Empty, CT_GenericPrinter, CT_Uthernet2}},
        {2, {CT_Empty, CT_SSC, CT_Uthernet2}},
        {3, {CT_Empty, CT_Uthernet, CT_Uthernet2, CT_VidHD}},
        {4, {CT_Empty, CT_MockingboardC, CT_MegaAudio, CT_SDMusic, CT_MouseInterface, CT_Phasor, CT_Uthernet2}},
        {5,
         {CT_Empty, CT_MockingboardC, CT_MegaAudio, CT_SDMusic, CT_Disk2, CT_GenericHDD, CT_Phasor, CT_Uthernet2,
          CT_Z80, CT_SAM, CT_FourPlay, CT_SNESMAX}},
        {6, {CT_Empty, CT_Disk2, CT_Uthernet2}},
        {7, {CT_Empty, CT_GenericHDD, CT_Uthernet2}},
    };

    const std::vector<SS_CARDTYPE> expansionCards = {
        CT_Empty, CT_LanguageCard, CT_Extended80Col, CT_Saturn128K, CT_RamWorksIII};

    uint8_t roundToRGB(float x)
    {
        // c++ cast truncates
        return uint8_t(x * 255 + 0.5);
    }

} // namespace

namespace sa2
{

    const std::string &getCardName(SS_CARDTYPE card)
    {
        return cards.at(card);
    }

    const std::string &getCPUName(eCpuType cpu)
    {
        return cpuTypes.at(cpu);
    }

    const std::string &getAppModeName(AppMode_e mode)
    {
        return appModes.at(mode);
    }

    const std::vector<SS_CARDTYPE> &getCardsForSlot(size_t slot)
    {
        return cardsForSlots.at(slot);
    }

    const std::vector<SS_CARDTYPE> &getExpansionCards()
    {
        return expansionCards;
    }

    const std::map<SS_CARDTYPE, std::string> &getCardNames()
    {
        return cards;
    }

    const std::map<eApple2Type, std::string> &getAapple2Types()
    {
        return apple2Types;
    }

    const std::map<DONGLETYPE, std::string> &getDongleTypes()
    {
        return dongleTypes;
    }

    const std::map<VideoType_e, std::string> &getVideoTypes()
    {
        return videoTypes;
    }

    const std::string &getDiskStatusName(Disk_Status_e status)
    {
        return statuses.at(status);
    }

    void insertCard(size_t slot, SS_CARDTYPE card, FrameBase *frame)
    {
        CardManager &cardManager = GetCardMgr();
        Video &video = GetVideo();
        const bool oldHasVid = video.HasVidHD();
        switch (slot)
        {
        case 3:
        {
            if (cardManager.QuerySlot(slot) == CT_VidHD)
            {
                // the old card was a VidHD, which will be removed
                // reset it
                video.SetVidHD(false);
            }
            break;
        }
        };

        cardManager.Insert(slot, card);

        // keep everything consistent
        // a bit of a heavy call, but nothing simpler is available now
        MemInitializeIO();

        if (oldHasVid != video.HasVidHD())
        {
            frame->Destroy();
            frame->Initialize(true);
        }
    }

    void setExpansionCard(SS_CARDTYPE card)
    {
        SetExpansionMemType(card);
        CreateLanguageCard();
        MemInitializeIO();
    }

    void setVideoStyle(Video &video, const VideoStyle_e style, const bool enabled)
    {
        VideoStyle_e currentVideoStyle = video.GetVideoStyle();
        if (enabled)
        {
            currentVideoStyle = VideoStyle_e(currentVideoStyle | style);
        }
        else
        {
            currentVideoStyle = VideoStyle_e(currentVideoStyle & (~style));
        }
        video.SetVideoStyle(currentVideoStyle);
    }

    void saveTFEEnabled(const int enabled)
    {
        REGSAVE(REGVALUE_UTHERNET_ACTIVE, enabled);
    }

    void changeBreakpoint(const uint32_t nAddress, const bool enableAndSet)
    {
        // see _BWZ_RemoveOne
        for (Breakpoint_t &bp : g_aBreakpoints)
        {
            if (bp.bSet && bp.nLength && (nAddress >= bp.nAddress) && (nAddress < bp.nAddress + bp.nLength))
            {
                bp.bSet = enableAndSet;
                bp.bEnabled = enableAndSet;
                if (!enableAndSet)
                {
                    bp.nLength = 0;
                    --g_nBreakpoints;
                }
            }
        }
    }

    ImVec4 colorrefToImVec4(const COLORREF cr)
    {
        const float coeff = 1.0 / 255.0;
        const bgra_t *bgra = reinterpret_cast<const bgra_t *>(&cr);
        const ImVec4 color(bgra->b * coeff, bgra->g * coeff, bgra->r * coeff, 1);
        return color;
    }

    COLORREF imVec4ToColorref(const ImVec4 &color)
    {
        const bgra_t bgra = {roundToRGB(color.x), roundToRGB(color.y), roundToRGB(color.z), roundToRGB(color.w)};
        const COLORREF *cr = reinterpret_cast<const COLORREF *>(&bgra);
        return *cr;
    }

    char getPrintableChar(const uint8_t x) // copied from FormatCharTxtCtrl
    {
        char c = x & 0x7F; // .32 Changed: Lo now maps High Ascii to printable chars. i.e. ML1 D0D0
        if (c < 0x20)      // SPACE
        {
            c += '@'; // map ctrl chars to visible
        }
        return c;
    }

    void sectableToClipboard(const std::string &text)
    {
        if (!text.empty() && ImGui::Selectable(text.c_str()))
        {
            SDL_SetClipboardText(text.c_str());
        }
    }

} // namespace sa2

namespace ImGui
{

    bool CheckBoxTristate(const char *label, int *v_tristate)
    {
        bool ret;
        if (*v_tristate == -1)
        {
            ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
            bool b = false;
            ret = ImGui::Checkbox(label, &b);
            if (ret)
                *v_tristate = 1;
            ImGui::PopItemFlag();
        }
        else
        {
            bool b = (*v_tristate != 0);
            ret = ImGui::Checkbox(label, &b);
            if (ret)
                *v_tristate = (int)b;
        }
        return ret;
    }

    void PushStyleCompact()
    {
        ImGuiStyle &style = ImGui::GetStyle();
        ImGui::PushStyleVar(
            ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, (float)(int)(style.FramePadding.y * 0.60f)));
        ImGui::PushStyleVar(
            ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, (float)(int)(style.ItemSpacing.y * 0.60f)));
    }

    void PopStyleCompact()
    {
        ImGui::PopStyleVar(2);
    }

} // namespace ImGui
