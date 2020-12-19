#pragma once

#include <memory>

#include <ncurses.h>

class GraphicsColors;

class Frame
{
 public:
  Frame();

  WINDOW * getWindow();
  WINDOW * getStatus();

  void init(int rows, int columns);
  int getColumns() const;

  static GraphicsColors & getColors();

  static void unInitialise();

 private:

  int myRows;
  int myColumns;

  std::shared_ptr<WINDOW> myFrame;
  std::shared_ptr<WINDOW> myStatus;

  static std::shared_ptr<GraphicsColors> ourColors;
  static bool ourInitialised;
  static void initialise();
};
