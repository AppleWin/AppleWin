#!/bin/bash

set -euxo pipefail

wget https://raw.githubusercontent.com/libretro/RetroArch/master/libretro-common/include/libretro.h -P libretro-common/include

git clone --depth=1 https://github.com/ocornut/imgui.git

mkdir -p build
cd build

cmake -DCMAKE_BUILD_TYPE=RELEASE -DLIBRETRO_COMMON_PATH=../libretro-common -DIMGUI_PATH=../imgui ..
make
