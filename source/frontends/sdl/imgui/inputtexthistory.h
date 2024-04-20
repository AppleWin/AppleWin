#pragma once

#include "Debugger/Debug.h"

#include <vector>
#include <string>

struct ImGuiInputTextCallbackData;

namespace sa2
{

  class InputTextHistory
  {
  public:
    InputTextHistory();

    bool inputText(const char* label);
    std::string execute();

  private:
    static constexpr size_t myMaximumSize = 20;
    static int inputTextCallbackStub(ImGuiInputTextCallbackData* data);

    void insertNext();
    char myInputBuffer[CONSOLE_WIDTH] = "";
    int inputTextCallback(ImGuiInputTextCallbackData* data);

    size_t myHistoryPos;
    std::vector<std::string> myHistory;
  };

}
