#include "frontends/ncurses/world.h"
#include "StdAfx.h"

#include <ncurses.h>
#include <signal.h>

#include "linux/keyboardbuffer.h"

#include "frontends/ncurses/nframe.h"

// #define KEY_LOGGING_VERBOSE

#ifdef KEY_LOGGING_VERBOSE
#include "Log.h"
#endif

namespace
{

  void sig_handler_pass(int signo)
  {
    // Ctrl-C
    // is there a race condition here?
    // is it a problem?
    addKeyToBuffer(0x03);
  }

  void sig_handler_exit(int signo)
  {
    na2::g_stop = true;
  }

}


namespace na2
{

  bool g_stop = false;

  void SetCtrlCHandler(const bool headless)
  {
    if (headless)
    {
      signal(SIGINT, sig_handler_exit);
    }
    else
    {
      signal(SIGINT, sig_handler_pass);
      // pass Ctrl-C to the emulator
    }
  }

  int GetKeyPressed(const std::shared_ptr<NFrame> & frame)
  {
    WINDOW * window = frame->GetWindow();
    if (!window)
    {
      return ERR;
    }

    const int inch = wgetch(window);

#ifdef KEY_LOGGING_VERBOSE
    if (inch != ERR)
    {
      LogFileOutput("wgetch = %3d\n", inch);
    }
#endif

    int ch = ERR;

    /*
    Ctrl & Alt - Arrows are not quite right.
    I have changed them many times and I suspect they are terminal-dependent.
    */

    switch (inch)
    {
    case ERR:
      break;
    case '\n':
      ch = 0x0d; // ENTER
      break;
    case KEY_BACKSPACE:
    case KEY_LEFT:
      ch = 0x08;
      break;
    case KEY_RIGHT:
      ch = 0x15;
      break;
    case KEY_UP:
      ch = 0x0b;
      break;
    case KEY_DOWN:
      ch = 0x0a;
      break;
    case 0x14a: // DEL
      ch = 0x7f;
      break;
    case 410:
      frame->ReInit();
      break;
    case 548: // Raspberry Pi
    case 550: // Alt-Left
    case 552: // Ctrl-Left
      frame->ChangeColumns(-1);
      break;
    case 563: // Raspberry Pi
    case 565: // Alt-Right
    case 567: // Ctrl-Right
      frame->ChangeColumns(+1);
      break;
    case 569: // Raspberry Pi
    case 571: // Alt-Up
    case 573: // Ctrl-Up
      frame->ChangeRows(-1);
      break;
    case 528: // Raspberry Pi
    case 530: // Alt-Down
    case 532: // Ctrl-Down
      frame->ChangeRows(+1);
      break;
    default:
      if (inch < 0x80)
      {
        ch = inch;
        // Standard for Apple II is Upper case
        if (ch >= 'A' && ch <= 'Z')
        {
          ch += 'a' - 'A';
        }
        else if (ch >= 'a' && ch <= 'z')
        {
          ch -= 'a' - 'A';
        }
      }
    }

    if (ch != ERR)
    {
      addKeyToBuffer(ch);
      return ERR;
    }
    else
    {
      // pass it back
      return inch;
    }
  }

}
