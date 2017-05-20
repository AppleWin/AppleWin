#include "frontends/ncurses/frame.h"

Frame::Frame() : myColumns(-1)
{
  init(40);

  const int rows = 10;
  const int cols = 40;

  const int xpos = 0;
  const int ypos = 26;

  myBuffer.reset(newwin(rows, cols, ypos + 1, xpos + 1), delwin);
  scrollok(myBuffer.get(), true);
  wrefresh(myBuffer.get());

  myBorders.reset(newwin(1 + rows + 1, 1 + cols + 1, ypos, xpos), delwin);
  box(myBorders.get(), 0 , 0);
  wrefresh(myBorders.get());
}

void Frame::init(int columns)
{
  if (myColumns != columns)
  {
    if (columns < myColumns)
    {
      werase(myFrame.get());
      wrefresh(myFrame.get());
    }

    myColumns = columns;

    myFrame.reset(newwin(1 + 24 + 1, 1 + myColumns + 1, 0, 0), delwin);
    box(myFrame.get(), 0 , 0);
    wtimeout(myFrame.get(), 0);
    keypad(myFrame.get(), true);
    wrefresh(myFrame.get());
  }

}

WINDOW * Frame::getWindow()
{
  return myFrame.get();
}

WINDOW * Frame::getBuffer()
{
  return myBuffer.get();
}

int Frame::getColumns() const
{
  return myColumns;
}
