# docker run -it --rm -v "AppleWin:/build" git.libretro.com:5050/libretro-infrastructure/libretro-build-amd64-ubuntu:xenial-gcc9 bash
# docker run -it --rm -v "AppleWin:/build" git.libretro.com:5050/libretro-infrastructure/libretro-build-mxe-win-cross-cores:gcc11 bash
apt-get update -qy
apt-get upgrade -qy
git clone https://github.com/audetto/AppleWin.git --depth=1
cd AppleWin
apt-get -qy install $(cat source/frontends/libretro/xenial/packages.txt)
cmake -DBUILD_LIBRETRO=ON -B build -G Ninja
cmake --build build
