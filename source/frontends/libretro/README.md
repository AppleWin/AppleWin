# libretro core

There is a [libretro](https://docs.libretro.com/development/cores/developing-cores/) core.

## RetroPad

A retropad can be plugged in port 1 (with or without analog stick).

| Device | Axis | ID | Default action |
| ------ | ------ | ------- | ---- |
| Standard | | A | Button 0 |
| Standard | | B | Button 1 |
| Standard | | Left | PDL(0) = 0 |
| Standard | | Right | PDL(0) = 255 |
| Standard | | Up | PDL(1) = 0 |
| Standard | | Down | PDL(1) = 255 |
| Standard | | L |  |
| Standard | | R |  |
| Standard | | L2 |  |
| Standard | | R2 |  |
| Standard | | L3 |  |
| Standard | | R3 |  |
| Analog | Left | X | PDL(0) |
| Analog | Left | Y | PDL(1) |
| Mouse  | | Left | Button 0 |
| Mouse  | | LeftRight | Button 1 |
| Mouse  | Position | | PDLs |

All *standard* buttons can be remapped to keys (in which case they lose their default action): buttons are remapped on all connected devices.

The speed of the mouse pointer can be tuned with an option.

## Keyboard

* ``END``: equivalent to ``F2`` to reset the machine
* ``HOME``: save configuration to `/tmp/applewin.retro.conf`

In order to have a better experience with the keyboard, one should probably enable *Game Focus Mode* (normally Scroll-Lock) to disable hotkeys. Even better set *Auto Enable 'Game Focus' Mode* to *Detect*.

## Misc

Easiest way to run from the ``build`` folder:
``retroarch -L source/frontends/libretro/applewin_libretro.so ../bin/MASTER.DSK``

It supports playlists files `.m3u` (see https://docs.libretro.com/library/vice/#m3u-and-disk-control alttough not all options are implemented).

The core can be statically linked in Linux and MSYS2, pass `-DSTATIC_LINKING=ON` to `cmake`.
