#!/bin/bash

set -euxo pipefail

mkdir -p build
cd build

cmake -DCMAKE_BUILD_TYPE=RELEASE ..
make -j 2
