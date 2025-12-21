# MSYS2 support

## Status

`applen` and `applewin_libretro` work on the *mingw64* environment of MSYS2.

## Building

Very very **important** to install a `mingw-w64-x86_64` toolchain.

Then configure as usual: `cmake .. -G Ninja` or your preferred version of `make`.

## Limitations

`static` linking is supported on mys2 and Ubuntu, pass `-DSTATIC_LINKING=ON` to `cmake`.

On Ubuntu `libz` cannot be statically linked.


## ssh access

Start a terminal with

`C:/msys64/msys2_shell.cmd -defterm -here -no-start -mingw64`

## Configuration files

`applen` stores its settings in `%LOCALAPPDATA%\applewin\applewin.yaml`
