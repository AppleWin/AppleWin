cmake_minimum_required(VERSION 3.13)

project(applewin HOMEPAGE_URL "https://github.com/audetto/AppleWin")

option(BUILD_APPLEN   "build ncurses frontend")
option(BUILD_QAPPLE   "build Qt5 frontend")
option(BUILD_SA2      "build SDL2 frontend")
option(BUILD_LIBRETRO "build libretro core")

if (NOT (BUILD_APPLEN OR BUILD_QAPPLE OR BUILD_SA2 OR BUILD_LIBRETRO))
  message(NOTICE "Building everything by default")
  set(BUILD_APPLEN ON)
  set(BUILD_QAPPLE ON)
  set(BUILD_SA2 ON)
  set(BUILD_LIBRETRO ON)
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Werror=return-type)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-Werror=format -Wno-error=format-overflow -Wno-error=format-truncation -Wno-psabi)
endif()

MESSAGE("CMAKE_BUILD_TYPE:                ${CMAKE_BUILD_TYPE}")
MESSAGE("CMAKE_CXX_FLAGS:                 ${CMAKE_CXX_FLAGS}")
MESSAGE("CMAKE_CXX_FLAGS_RELEASE:         ${CMAKE_CXX_FLAGS_RELEASE}")
MESSAGE("CMAKE_CXX_FLAGS_DEBUG:           ${CMAKE_CXX_FLAGS_DEBUG}")
MESSAGE("CMAKE_CXX_FLAGS_RELWITHDEBINFO:  ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

# this only affects common2, the others are already build with fPIC by default
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

execute_process(COMMAND uname -n
  OUTPUT_VARIABLE UNAME
  OUTPUT_STRIP_TRAILING_WHITESPACE)

if(${UNAME} STREQUAL raspberrypi)
  # it is too slow and might cause out of memory issues
  # more forensic is required
  MESSAGE(NOTICE "Raspberry Pi detected: IPO disabled")
else()
  include(CheckIPOSupported)
  check_ipo_supported()
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
endif()

include_directories(source)

add_subdirectory(source)
add_subdirectory(source/linux/libwindows)
add_subdirectory(test/TestCPU6502)

if (BUILD_LIBRETRO OR BUILD_APPLEN OR BUILD_SA2)
  add_subdirectory(source/frontends/common2)
endif()

if (BUILD_APPLEN)
  add_subdirectory(source/frontends/ncurses)
endif()

if (BUILD_QAPPLE)
  add_subdirectory(source/frontends/qt)
endif()

if (BUILD_SA2)
  add_subdirectory(source/frontends/sdl)
endif()

if (BUILD_LIBRETRO)
  add_subdirectory(source/frontends/libretro)
endif()

file(STRINGS resource/version.h VERSION_FILE LIMIT_COUNT 1)
string(REGEX MATCH "#define APPLEWIN_VERSION (.*)" _ ${VERSION_FILE})
string(REPLACE "," "." VERSION ${CMAKE_MATCH_1})

set(CPACK_PACKAGE_VERSION ${VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Apple ][ emulator for Linux")
set(CPACK_PACKAGE_CONTACT "audetto <mariofutire@gmail.com>")

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libyaml-0-2,libminizip1,libqt5gui5,libqt5widgets5,libqt5multimedia5,libqt5gamepad5,libncursesw6,libevdev2,libsdl2-image-2.0-0,libsdl2-2.0-0,libgles2,libpcap0.8,libslirp0,libboost-program-options1.74.0")

set(CPACK_RPM_PACKAGE_LICENSE "GPLv2")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Emulators")
# RPM dependencies are automatic

include(CPack)
