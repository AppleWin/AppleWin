#include "frontends/ncurses/world.h"
#include "StdAfx.h"

#include <ncurses.h>
#include <signal.h>

#include "linux/linuxinterface.h"
#include "linux/keyboard.h"

#include "frontends/ncurses/nframe.h"


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

  double g_relativeSpeed = 1.0;
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

  int ProcessKeyboard(const std::shared_ptr<NFrame> & frame)
  {
    WINDOW * window = frame->GetWindow();
    if (!window)
    {
      return ERR;
    }

    const int inch = wgetch(window);

    int ch = ERR;

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
    case 543 ... 546: // Various values for Ctrl/Alt - Left on Ubuntu and Pi OS
      frame->ChangeColumns(-1);
      break;
    case 558 ... 561: // Ctrl/Alt - Right
      frame->ChangeColumns(+1);
      break;
    case 564 ... 567: // Ctrl/Alt - Up
      frame->ChangeRows(-1);
      break;
    case 523 ... 526: // Ctrl/Alt - Down
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

// Mockingboard
void registerSoundBuffer(IDirectSoundBuffer * buffer)
{
}

void unregisterSoundBuffer(IDirectSoundBuffer * buffer)
{
}
