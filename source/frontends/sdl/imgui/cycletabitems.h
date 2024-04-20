#pragma once

#include <optional>

namespace sa2
{

  class CycleTabItems
  {
  public:
    void init();
    bool beginTabBar(const char * str_id);
    bool beginTabItem(const char * label);

    void prev();
    void next();
  private:
    size_t myNumberOfTabs = 0;
    size_t mySelectedTab;
    size_t myTabID;
    std::optional<size_t> myNextToSelect;
  };

}