# SA2: Apple ][ Emulator for Linux based on SDL2

This file only lists options not already described in ``-h``.

The default frontend uses SDL2 + ImGui and requires an OpenGL ES2.0 implementation. It is possible to use plain SDL2 and select the renderer (pass ``--no-imgui`` and look at ``-h``).

## Network and port forwarding

When `libslirp` is used as backend for Uthernet cards, ports must be explicitly opened and forwarded to the guest (only MACRAW sockets for Uthernet 2).

Syntax is similar to Virtual Box
```
--nat 3,tcp,,8080,,http
```
Multiple `nat` options can be specified at once

- default host address is 0.0.0.0
- default guest address is the first in the dhcp range
- service names are accepted
- the slot (3) is currently ignored

Parsing errors are fatal, listening errors are logged to the console.

## Configuration

The configuration GUI only works with ImGui: otherwise either manually edit the configuration file ``~/.applewin/applewin.conf`` or use ``qapple`` and run ``sa2 --qt-ini``.
The format of the configuration file is the same as the Windows Registry of AppleWin.

*Drag & drop* works for floppy disks. With ImGui it is possible to select which drive they are dropped into (`System` -> `Settings` -> `Hardware` -> `D&D`).
If the filename ends with `.yaml`, it will be loaded as a *State* file.

Individual options can be passed via arguments too: ``-r Configuration.Printer_Filename=Printer.txt``.

If you have a modern gamepad where the axes (``LEFTX`` and ``LEFTY``) move in a circle, the emulator will automatically map to a square: use ``--no-squaring`` to avoid this.

## Dear ImGui

The rendering is performed with [Dear ImGui](https://github.com/ocornut/imgui).

On a Raspberry Pi, a KMS driver is mandatory and best results are obtained on a Pi4 with FullKMS (full screen via ``F6``).

Output mentions the Dear ImGui version, e.g.: ``IMGUI_VERSION: 1.81 WIP``.

Some of the configuration options are exposed in the ``Settings`` menu. This is **not** a modal dialog and options are applied immediately. You might need to **Restart** the emulator manually.

## Hotkeys

``F2``, ``F5``, ``F6``, ``F9``, ``F11``, ``F12`` and ``Pause``  have the same meaning as in AppleWin.

``Left Alt`` and ``Right Alt`` emulate the Open and Solid Apple key.

``Shift-Insert`` pastes the clipboard to the input key buffer.

``Ctrl-Insert`` copies the text screen (in AppleWin this is ``Ctrl-PrintScreen``).

## Audio

Audio works reasonably well, using AppleWin adaptive algorithm.

There is a command line argument to customise the SDL audio buffer: ``--audio-buffer 46``.
AppleWin target is between 92 and 185 ms, so any number above 90 will risk numerous underruns. It can be as small as 1, but it will probably put pressure on the host scheduling.

Use ``F1`` during emulation to have an idea of the size of the audio queue

```
Channels: 2, buffer:  21780,   123.47 ms, underruns:          0
Channels: 1, buffer:   9760,   110.66 ms, underruns:          1
```
(1) is the speaker, (2) the Mockingboard.

## Speed diagnostic

At the end of the run, it will print stats about timings:
```
Video refresh rate: 60 Hz, 16.67 ms
Global:  total =    7789.16 ms, mean =    7789.16 ms, std =       0.00 ms, n =      1
Events:  total =      22.42 ms, mean =       0.05 ms, std =       0.17 ms, n =    471
Texture: total =     113.32 ms, mean =       0.24 ms, std =       0.06 ms, n =    471
Screen:  total =    7624.87 ms, mean =      16.19 ms, std =       1.66 ms, n =    471
CPU:     total =     647.21 ms, mean =       1.34 ms, std =       0.48 ms, n =    484
```

- ``events``: SDL events and audio
- ``texture``: ``SDL_UpdateTexture``
- ``screen``: ``SDL_RenderCopyEx`` and ``SDL_RenderPresent`` (this includes ``vsync``)
- ``cpu``: AW's code

## Debugging

For debugging and profiling (valgrind), it is best to switch off adaptive speed, as otherwise it enters a feedback loop and seems to hang.

Use ``--fixed-speed``.
