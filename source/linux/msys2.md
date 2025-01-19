# MSYS2 support

## Status

`applen` and `applewin_libretro` work on the *mingw64* environment of MSYS2.

## Building

Very very **important** to install a `mingw-w64-x86_64` toolchain.

Then configure as usual: `cmake .. -G Ninja` or your preferred version of `make`.

## Limitations

`static` linking is not supported yet, so the applications (including `retroarch`) must be run from inside a MSYS2 terminal.

## ssh access

Start a terminal with

`C:/msys64/msys2_shell.cmd -defterm -here -no-start -mingw64`
