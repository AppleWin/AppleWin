# SSI263 synthesis tests

Build from the repository root with a C++14 compiler:

```sh
g++ -std=c++14 -O3 -Wall -Wextra -Werror -DSSI263_SYNTH_TEST source/SSI263Synth.cpp test/SSI263Synth/TestSSI263Synth.cpp -o TestSSI263Synth
./TestSSI263Synth
```

Or use a Visual Studio developer command prompt:

```bat
cl /nologo /std:c++14 /O2 /EHsc /W4 /WX /DSSI263_SYNTH_TEST source\SSI263Synth.cpp test\SSI263Synth\TestSSI263Synth.cpp /Fe:TestSSI263Synth.exe
TestSSI263Synth.exe
```

The tests cover cold reset, register aliases, independent chips, split clock
advances, exact snapshot continuation, invalid snapshots, and all phonemes
with extreme register settings. Function restore preserves running synthesis
state and selects immediate or transitioned pitch. A cold snapshot also checks
the first speech samples after one second of silence. The tests use no Windows
or audio device APIs.

`RenderTrace.cpp` can compare PCM with another implementation. Build it with
the same command, replacing `TestSSI263Synth.cpp` and the output name with
`RenderTrace.cpp` and `RenderTrace`. Run `RenderTrace events.txt output.pcm`.
The output is signed 16-bit little-endian stereo PCM at 48 kHz, with socket 0
on the left and socket 1 on the right.

The trace is plain text. Its first line is:

```text
SSIHOST1 clock_hz start_tick end_tick event_count
```

Each following line contains `tick socket register value`, in time order.
All numbers are decimal. Clock ticks use the effective SSI clock, after any
board clock divider. Negative ticks allow setup before the first saved sample.
Writes at a sample boundary take effect before that sample. Earlier samples
still run when `start_tick` selects a later range.

These tests check the digital model. They do not establish agreement with a
physical SSI263, nor test the AppleWin audio mixer or interrupt timing.
