#include "frontends/sdl/imgui/cycletabitems.h"
#include "imgui.h"

#include <algorithm>

namespace sa2
{

    void CycleTabItems::init()
    {
        myTabID = 0;
        myNextToSelect.reset();
    }

    bool CycleTabItems::beginTabBar(const char *str_id)
    {
        return ImGui::BeginTabBar(str_id);
    }

    bool CycleTabItems::beginTabItem(const char *label)
    {
        const bool isSelected = myNextToSelect.has_value() && myNextToSelect.value() == myTabID;
        const ImGuiTabItemFlags tabFlags = isSelected ? ImGuiTabItemFlags_SetSelected : 0;

        const bool res = ImGui::BeginTabItem(label, nullptr, tabFlags);
        if (res)
        {
            mySelectedTab = myTabID;
        }
        ++myTabID;
        myNumberOfTabs = std::max(myNumberOfTabs, myTabID);
        return res;
    }

    void CycleTabItems::prev()
    {
        if (mySelectedTab == 0)
        {
            mySelectedTab = myNumberOfTabs;
        }

        myNextToSelect = mySelectedTab - 1;
    }

    void CycleTabItems::next()
    {
        myNextToSelect = (mySelectedTab + 1) % myNumberOfTabs;
    }

} // namespace sa2
