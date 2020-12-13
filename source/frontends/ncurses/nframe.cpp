#include "frontends/ncurses/nframe.h"

Frame::Frame() : myColumns(-1), myRows(-1)
{
  init(24, 40);
}

void Frame::init(int rows, int columns)
{
  if (myRows != rows || myColumns != columns)
  {
    if (columns < myColumns || rows < myRows)
    {
      werase(myStatus.get());
      wrefresh(myStatus.get());
      werase(myFrame.get());
      wrefresh(myFrame.get());
    }

    myRows = rows;
    myColumns = columns;

    const int width = 1 + myColumns + 1;
    const int left = (COLS - width) / 2;

    myFrame.reset(newwin(1 + myRows + 1, width, 0, left), delwin);
    box(myFrame.get(), 0 , 0);
    wtimeout(myFrame.get(), 0);
    keypad(myFrame.get(), true);
    wrefresh(myFrame.get());

    myStatus.reset(newwin(4, width, 1 + myRows + 1, left), delwin);
    box(myStatus.get(), 0 , 0);
    wrefresh(myStatus.get());
  }

}

WINDOW * Frame::getWindow()
{
  return myFrame.get();
}

WINDOW * Frame::getStatus()
{
  return myStatus.get();
}

int Frame::getColumns() const
{
  return myColumns;
}
