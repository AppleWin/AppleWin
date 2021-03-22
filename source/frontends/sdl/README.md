# SA2: Apple ][ Emulator for Linux based on SDL2

This file only lists options not already described in ``-h``.

## Configuration

There is no GUI to configure the emulator: either manually edit the configuration file ``~/.applewin/applewin.conf`` or use ``qapple`` and run ``sa2 --qt-ini``.
The format of the configuration file is the same as the Windows Registry of AppleWin.

Individual options can be passed via arguments too: ``-c Configuration.Printer_FileName=Printer.txt``.

If you have a modern gamepad where the axes (``LEFTX`` and ``LEFTY``) move in a circle, the emulator will automatically map to a square: use ``--no-squaring`` to avoid this.

## Raspberry Pi

On a Raspberry Pi, one needs the KMS (fake or not). Better performance has been observed with the ``opengles2`` driver (use ``sa2 --sdl-driver 1``).

## Dear ImGui

With the flag ``--imgui``, the rendering is performed with [Dear ImGui](https://github.com/ocornut/imgui). In this case ``--sdl-driver`` is ignored and a OpenGL ES2.0 implementation is required.

On a Raspberry Pi, a KMS driver is mandatory and best results are obtained on a Pi4 with FullKMS (full screen via ``F6``).

Output mentions the Dear ImGui version, e.g.: ``IMGUI_VERSION: 1.81 WIP``.

There is a rudimental suppor for configuration: *drag & drop* works for floppy disks, and they are inserted in Drive 1.

## Hotkeys

``F2``, ``F5``, ``F6``, ``F9``, ``F11``, ``F12`` and ``Pause``  have the same meaning as in AppleWin.

``Left Alt`` and ``Right Alt`` emulate the Open and Solid Apple key.

## Audio

Audio works reasonably well, using AppleWin adaptive algorithm.

Use ``F1`` during emulation to have an idea of the size of the audio queue

```
Channels: 1, buffer: 32768, SDL:  8804, queue: 0.47 s
Channels: 2, buffer: 45000, SDL: 65536, queue: 0.63 s
```
(1) is the speaker, (2) the Mockingboard.

Speech / phonemes are known to hang the emulator.

## Speed diagnostic

At the end of the run, it will print stats about timings:
```
Video refresh rate: 60 Hz, 16.67 ms
Global:  total =    7789.16 ms, mean =    7789.16 ms, std =       0.00 ms, n =      1
Events:  total =      22.42 ms, mean =       0.05 ms, std =       0.17 ms, n =    471
Texture: total =     113.32 ms, mean =       0.24 ms, std =       0.06 ms, n =    471
Screen:  total =    7624.87 ms, mean =      16.19 ms, std =       1.66 ms, n =    471
CPU:     total =     647.21 ms, mean =       1.34 ms, std =       0.48 ms, n =    484
Expected clock: 1020484.45 Hz, 7.74 s
Actual clock:   1014560.11 Hz, 7.79 s
```

- ``events``: SDL events and audio
- ``texture``: ``SDL_UpdateTexture``
- ``screen``: ``SDL_RenderCopyEx`` and ``SDL_RenderPresent`` (this includes ``vsync``)
- ``cpu``: AW's code

The clock shows expected vs actual speed.

## Debugging

For debugging and profiling (valgrind), it is best to switch off adaptive speed, as otherwise it enters a feedback loop and seems to hang.

Use ``--fixed-speed``.
