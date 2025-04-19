# MXE

This is the cross compiler version of the [msys2](msys2.md).

## Setup

https://mxe.cc

Tested with: `MXE_TARGETS := x86_64-w64-mingw32.static`

`make cc cmake boost ncurses`

`xxd` must be available on the host.

## Build

`LIBRETRO` and `APPLEN` are supported.

`x86_64-w64-mingw32.static-cmake -DBUILD_LIBRETRO=ON -G Ninja`

## libretro docker image

```
docker run -it --rm -v "$(pwd):/build" git.libretro.com:5050/libretro-infrastructure/libretro-build-mxe-win-cross-cores:gcc11 bash
cd /usr/lib/mxe
make boost
apt update
apt install xxd
/usr/lib/mxe/usr/bin/x86_64-w64-mingw32.static-cmake -DBUILD_LIBRETRO=ON -B build -G Ninja
```
