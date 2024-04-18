#include "StdAfx.h"
#include "frontends/sdl/imgui/sdlmemory.h"
#include "frontends/sdl/imgui/settingshelper.h"
#include "imgui.h"

#include "Memory.h"
#include "Debugger/DebugDefs.h"
#include "StrFormat.h"

#include <iomanip>

namespace sa2
{

  void ImGuiMemory::drawSingleView(const size_t address)
  {
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("table1", 3, flags))
    {
      ImGuiListClipper clipper;
      clipper.Begin(myNumberOfRows);
      while (clipper.Step())
      {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
        {
          const size_t base = (address + row * myBytesPerRow) & _6502_MEM_END;
          const std::string addr = StrFormat("%04X", (unsigned)base);

          std::ostringstream hex;
          hex << std::setfill('0') << std::hex << std::uppercase;

          std::ostringstream text;
          for (size_t k = 0; k < myBytesPerRow; ++k)
          {
            if (k)
            {
              hex << ' ';
            }
            const int value = static_cast<int>(mem[(base + k) & _6502_MEM_END]);
            hex << std::setw(2) << value;
            text << getPrintableChar(value);
          }

          ImGui::TableNextColumn();
          ImGui::Selectable(addr.c_str(), false, ImGuiSelectableFlags_SpanAllColumns);

          ImGui::TableNextColumn();
          ImGui::TextUnformatted(hex.str().c_str());

          ImGui::TableNextColumn();
          ImGui::TextUnformatted(text.str().c_str());
        }
      }
      ImGui::EndTable();
    }
  }

  void ImGuiMemory::draw()
  {
    if (ImGui::Begin("Memory viewer", &show))
    {
      ImGui::PushItemWidth(ImGui::GetFontSize() * 5);
      ImGui::InputScalar("address", ImGuiDataType_U16, &myNewAddress, nullptr, nullptr, "%04X", ImGuiInputTextFlags_CharsHexadecimal);
      ImGui::PopItemWidth();
      ImGui::SameLine();
      if (ImGui::Button("Add"))
      {
        myMemoryAddresses.push_back(myNewAddress);
        myNewAddress = (myNewAddress + 0x0100) & _6502_MEM_END;
      }

      ImGui::SliderInt("Width", &myBytesPerRow, 8, 32);

      auto it = myMemoryAddresses.begin();
      int id = 0;
      while (it != myMemoryAddresses.end())
      {
        ImGui::PushID(++id);
        ImGui::BeginChild("pippo", ImVec2(0, 200), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeY);
        if (ImGui::Button("Remove"))
        {
          it = myMemoryAddresses.erase(it);
        }
        else
        {
          drawSingleView(*it);
          ++it;
        }
        ImGui::EndChild();
        ImGui::PopID();
      }
    }
    ImGui::End();
  }

}
