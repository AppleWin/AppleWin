# AppleWin on Linux

This is a linux port of [AppleWin](https://github.com/AppleWin/AppleWin), that shares 100% of the code of the core emulator and video generation. Audio, UI, scheduling and other peripherals are **reimplemented**.

**Number one goal** is to stay source compatible with AppleWin, to make `git merge` a trivial operation. Shared files are modified as a last resort.

## Structure

There are 4 projects

* libapple: the core emulator files
* applen: a frontend based on ncurses
* qapple: Qt frontend
* sa2: SDL2 frontend
* a [libretro](https://www.libretro.com) core

##  What works

Almost everything works, except the serial port, SNES-MAX and FourPlay.

The UI has been rewritten in Qt or ImGui. The rest works very well.

[Network](/source/Tfe/README.md) is supported via [libslirp](https://gitlab.freedesktop.org/slirp/libslirp).

If this is not available, it uses `libpcap`, but it requires elevated capabilities:

`sudo setcap cap_net_raw=ep ./sa2`

Unfortunately, this must be reapplied after every build.

Most of the debugger now works (in the ImGui version).

## New features

Audio files can be read via the cassette interface (SDL Version). Just drop a `wav` file into the emulator. Tested with all the formats from [asciiexpress](https://asciiexpress.net/).

## Executables

### sa2

This is your best choice, in particular the ImGui version.

TL;DR: just run ``sa2``

See [sa2](/source/frontends/sdl/README.md) for more details.

### applen

Frontend based on ncurses, with a ASCII art graphic mode.

Keyboard shortcuts

* ``F2``: reset the machine
* ``F3``: terminate the emulator
* ``F11``, ``F12``: Save, Load Snapshot
* ``ALT-RIGHT``: wider hi res graphis
* ``ALT-LEFT``: narrower hi res graphics
* ``ALT-UP``: vertical hi res (smaller)
* ``ALT-DOWN``: vertical hi res (bigger)

In order to properly appreciate the wider hi res graphics, open a big terminal window and choose a small font size.
Try ``CTRL-`` as well if ``ALT-`` does not work: terminals do not report a consistent keycode for these combinations.

The joystick uses evdev (``--device-name /dev/input/by-id/id_of_device``).

### qapple

This is based on Qt.

* keyboard shortcuts are listed in the menu entries
* joystick: it uses QtGamepad
* emulator runs in the main UI thread
* Qt timers are very coarse: the emulator needs to dynamically adapt the cycles to execute
* the app runs at 60FPS with correction for uneven timer deltas.
* full speed when disk spins execute up to 5 ms real wall clock of emulator code (then returns to Qt)
* audio is supported and there are a few configuration options to tune the latency (default very conservative 200ms)
* Open Apple and Solid Apple can be emulated using AltGr and Menu (unfortunately, Alt does not work well)
* ``yaml`` files can be dropped to restore a saved state

### ra2

There is an initial [libretro](https://docs.libretro.com/development/cores/developing-cores/) core.

A retropad can be plugged in port 1 (with or without analog stick).

Keyboard emulation

* ``JOYPAD_R``: equivalent to ``F9`` to cycle video types
* ``JOYPAD_L``: equivalent to ``CTRL-SHIFT-F6`` to cycle 50% scan lines
* ``START``: equivalent to ``F2`` to reset the machine

In order to have a better experience with the keyboard, one should probably enable *Game Focus Mode* (normally Scroll-Lock) to disable hotkeys.

Video works, but the vertical flip is done in software.

Audio (speaker) works.

Easiest way to run from the ``build`` folder:
``retroarch -L source/frontends/libretro/applewin_libretro.so ../bin/MASTER.DSK``

It supports playlists files `.m3u` (see https://docs.libretro.com/library/vice/#m3u-and-disk-control alttough not all options are implemented).

## Build

The project can be built using cmake from the top level directory.

qapple can be managed from Qt Creator as well and the 2 have coexisted so far, but YMMV.

### Checkout

**Don't forget the submodules!!**

```
git clone https://github.com/audetto/AppleWin.git --recursive
cd AppleWin
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=RELEASE ..
make
```

### Frontend selection

There are 4 `cmake` variables to selectively enable frontends: `BUILD_APPLEN`, `BUILD_QAPPLE`, `BUILD_SA2` and `BUILD_LIBRETRO`.

Usage:

```
cmake -DBUILD_SA2=ON -DBUILD_LIBRETRO=ON ..
```

or use `cmake-gui` (if none is selected, they are all built).

### Fedora

On Fedora 35, from a fresh installation, install all packages from [fedora.list.txt](/source/linux/fedora.list.txt).

### Raspberry Pi OS, Ubuntu and other Debian distributions

Install all packages from [raspbian.list.txt](/source/linux/raspbian.list.txt).

You can use `sudo apt-get -y install $(cat raspbian.list.txt)` for an automated installation.

See [Travis](/.travis.yml) CI too.

### Packaging

It is possible to create `.deb` and `.rpm` packages using `cpack`. Use `cpack -G DEB` or `cpack -G RPM` from the build folder. It is best to build packages for the running system.

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
