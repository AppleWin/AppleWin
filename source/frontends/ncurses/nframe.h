#pragma once

#include <memory>

#include <ncurses.h>

class Frame
{
 public:
  Frame();

  WINDOW * getWindow();
  WINDOW * getStatus();

  void init(int rows, int columns);
  int getColumns() const;

 private:

  int myRows;
  int myColumns;

  std::shared_ptr<WINDOW> myFrame;
  std::shared_ptr<WINDOW> myStatus;

};
