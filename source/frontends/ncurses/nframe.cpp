#include "frontends/ncurses/nframe.h"
#include "frontends/ncurses/colors.h"

#include <signal.h>
#include <locale.h>
#include <stdlib.h>

bool Frame::ourInitialised = false;
std::shared_ptr<GraphicsColors> Frame::ourColors;

Frame::Frame() : myColumns(-1), myRows(-1)
{
  // only initialise if actually used
  // so we can run headless
}

void Frame::init(int rows, int columns)
{
  if (myRows != rows || myColumns != columns)
  {
    initialise();
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

void Frame::initialise()
{
  if (!ourInitialised)
  {
    setlocale(LC_ALL, "");
    initscr();

    // does not seem to be a problem calling endwin() multiple times
    std::atexit(Frame::unInitialise);

    curs_set(0);

    noecho();
    cbreak();
    set_escdelay(0);

    // make sure this happens when ncurses is indeed initialised
    ourColors.reset(new GraphicsColors(20, 20, 32));

    ourInitialised = true;
  }
}

void Frame::unInitialise()
{
  if (ourInitialised)
  {
    ourColors.reset();
    endwin();
    ourInitialised = false;
  }
}

GraphicsColors & Frame::getColors()
{
  return *ourColors;
}
