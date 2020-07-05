# Linux

* [Structure](#structure)
* [What works](#what-works)
* [Executables](#executables)
  * [applen](#applen)
  * [qapple](#qapple)
* [Build](#build)
  * [Checkout](#checkout)
  * [Fedora](#fedora)
  * [Raspbian](#raspbian)
* [Speed](#build)
  * [Fedora](#fedora-1)
  * [Raspbian](#raspbian-1)

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
* Frame.cpp
* Video.cpp (partially)
* Audio (including Mockingboard but excluding speech in QApple)

Some features totally ignored:

* ethernet
* serial port
* debugger
* speech (currently it hangs the emulator)

The rest is in a very usable state.

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
* graphics: runs the native NTSC code
* joystick: it uses QtGamepad (correct names will only be displayed with 5.11)
* emulator runs in the main UI thread
* Qt timers are very coarse: the emulator needs to dynamically adapt the cycles to execute
* the app runs at 60FPS with correction for uneven timer deltas.
* full speed when disk spins execute up to 5 ms real wall clock of emulator code (then returns to Qt)
* (standard) audio is supported and there are a few configuration options to tune the latency (default very conservative 200ms)
* plain mockingboard is supported as well (not speech, which hang the emulator)
* Open Apple and Closed Apple can be emulated using AltGr and Menu (unfortunately, Alt does not work well)

## Build

The project can be built using cmake from the top level directory.

qapple can be managed from Qt Creator as well and the 2 have coexisted so far, but YMMV.

### Checkout

```
git clone https://github.com/audetto/AppleWin.git --recursive
cd AppleWin
mkdir build
cd build
cmake ..
make
```
Use `cmake -DCMAKE_BUILD_TYPE=RELEASE` to get a *release* build.

### Fedora

On Fedora 31, from a fresh installation, install all packages from [fedora.list.txt](source/linux/fedora.list.txt).

### Raspbian

On Raspbian 10, from a fresh installation, install all packages from [raspbian.list.txt](source/linux/raspbian.list.txt).

Audio does not work and CPU utilisation is very high.

## Speed

### Fedora

Intel(R) Core(TM) i5-4460  CPU @ 3.20GHz

Full update = 582 MHz

| Video Stype | Video update |
| :--- | ---: |
| RGB Monitor | 39 |
| NTSC Monitor | 27 |
| Color TV | 25 |
| B&W TV | 27 |
| Amber Monitor | 31 |

### Raspbian

Pi 3B+

Full update = 54 MHz

| Video Stype | Video update |
| :--- | ---: |
| RGB Monitor | 5.3 |
| NTSC Monitor | 3.6 |
| Color TV | 2.6 |
| B&W TV | 2.9 |
| Amber Monitor | 4.5 |
