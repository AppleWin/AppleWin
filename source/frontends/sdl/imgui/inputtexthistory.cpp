#include "StdAfx.h"

#include "frontends/sdl/imgui/inputtexthistory.h"
#include "imgui.h"

namespace sa2
{

    InputTextHistory::InputTextHistory()
    {
        insertNext();
    }

    void InputTextHistory::insertNext()
    {
        // the last entry is the "live" one, being edited
        if (myHistory.empty() || !myHistory.back().empty())
        {
            myHistory.push_back("");
        }
        myHistoryPos = myHistory.size() - 1;
    }

    int InputTextHistory::inputTextCallbackStub(ImGuiInputTextCallbackData *data)
    {
        InputTextHistory *history = (InputTextHistory *)data->UserData;
        return history->inputTextCallback(data);
    }

    int InputTextHistory::inputTextCallback(ImGuiInputTextCallbackData *data)
    {
        switch (data->EventFlag)
        {
        case ImGuiInputTextFlags_CallbackHistory:
        {
            const size_t livePos = myHistory.size() - 1;
            const int prevHistoryPos = myHistoryPos;
            if (data->EventKey == ImGuiKey_UpArrow)
            {
                if (myHistoryPos > 0)
                {
                    --myHistoryPos;
                }
            }
            else if (data->EventKey == ImGuiKey_DownArrow)
            {
                if (myHistoryPos != livePos)
                {
                    ++myHistoryPos;
                }
            }

            if (prevHistoryPos != myHistoryPos)
            {
                if (prevHistoryPos == livePos)
                {
                    // save "live" editable entry
                    myHistory[livePos] = data->Buf;
                }
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, myHistory[myHistoryPos].c_str());
            }
            break;
        }
        }

        return 0;
    }

    std::string InputTextHistory::execute()
    {
        const std::string line = myInputBuffer;
        myInputBuffer[0] = 0;

        const auto end = myHistory.end() - 1; // do not compare with live, as it might match
        const auto it = std::find(myHistory.begin(), end, line);
        if (it != end)
        {
            myHistory.erase(it);
        }

        myHistory.back() = line; // replace "live" with the one executed

        if (myHistory.size() >= myMaximumSize)
        {
            myHistory.erase(myHistory.begin());
        }

        insertNext();

        return line;
    }

    bool InputTextHistory::inputText(const char *label)
    {
        const ImGuiInputTextFlags inputTextFlags = ImGuiInputTextFlags_EnterReturnsTrue |
                                                   ImGuiInputTextFlags_EscapeClearsAll |
                                                   ImGuiInputTextFlags_CallbackHistory;
        const bool result = ImGui::InputText(
            label, myInputBuffer, IM_ARRAYSIZE(myInputBuffer), inputTextFlags, &InputTextHistory::inputTextCallbackStub,
            this);
        return result;
    }

} // namespace sa2
