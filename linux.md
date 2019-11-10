# Linux

* [Structure](#structure)
* [What works](#what-works)
* [Executables](#executables)
  * [applen](#applen)
  * [qapple](#qapple)
* [Build](#build)
  * [Fedora](#fedora)

## Structure

There are 3 projects

* libapple: the core emulator files
* applen: a frontend based on ncurses
* qapple: Qt frontend

The libapple interface is a *link time* interface: some functions are not defined and must be provided in order to properly link
the application. These functions are listed in [interface.h](source/linux/interface.h).

The main goal is to reuse the AppleWin source files without changes: only where really necessary the AppleWin source files have
been modified, mostly for

* header files issues
* const char *
* exclude some Windows heavy blocks (source/MouseInterface.cpp)

##  What works

Some key files have been completely reimplemented or discarded:

* AppleWin.cpp
* Video.cpp
* Frame.cpp
* NTSC.cpp

Some features totally ignored:

* NSTC colors
* ethernet
* serial port
* debugger
* speech

The rest is in a usable state.

## Executables

### applen

Frontend based on ncurses, with a ASCII art graphic mode.

Keyboard shortcuts

* F2: terminate emulator
* F12: Load Snapshot
* ALT-RIGHT: wider hi res graphis
* ALT-LEFT: narrower hi res graphics
* ALT-UP: vertical hi res (smaller)
* ALT-DOWN: vertical hires (bigger)

In order to properly appreciate the wider hi res graphics, open a big terminal window and choose a small font size.

The joystick uses evdev (currently the device name is hardcoded).

### qapple

This is based on Qt, currently tested with 5.10

* keyboard shortcuts are listed in the menu entries
* graphics: code borrowed from linapple, no NTSC color
  * lo res in color
  * hi res in BW
* joystick: it uses QtGamepad (correct names will only be displayed with 5.11)
* emulator runs in the main UI thread
* Qt timers are very coarse: the emulator needs to dynamically adapt the cycles to execute
* the app runs at 60FPS with correction for uneven timer deltas.
* full speed when disk spins execute up to 5 ms real wall clock of emulator code (then returns to Qt)
* (standard) audio is supported and there are a few configuration options to tune the latency (default very conservative 200ms)

## Build

The project can be built using cmake from the top level directory.

qapple can be managed from Qt Creator as well and the 2 have coexisted so far, but YMMV.

### Fedora

On Fedora 31, from a fresh installation, install all packages from [fedora.list.txt](source/linux/fedora.list.txt).

Building with cmake works.

