#!/bin/bash

set -euxo pipefail

wget https://raw.githubusercontent.com/libretro/RetroArch/master/libretro-common/include/libretro.h -P libretro-common/include

mkdir -p build
cd build

cmake -DCMAKE_BUILD_TYPE=RELEASE -DLIBRETRO_COMMON_PATH=../libretro-common ..
make -j 4
