cmake_minimum_required(VERSION 3.3)

project(applewin)

add_library(appleii SHARED
  source/Pravets.cpp
  source/Tape.cpp
  source/YamlHelper.cpp
  source/Log.cpp
  source/Disk.cpp
  source/DiskImage.cpp
  source/DiskImageHelper.cpp
  source/Harddisk.cpp
  source/Memory.cpp
  source/CPU.cpp
  source/6821.cpp
  source/NoSlotClock.cpp
  source/SAM.cpp
  source/z80emu.cpp
  source/ParallelPrinter.cpp

  source/linux/data.cpp
  source/linux/dummies.cpp
  source/linux/wwrapper.cpp

  source/Z80VICE/z80.cpp
  source/Z80VICE/z80mem.cpp
  source/Z80VICE/daa.cpp
  )

target_link_libraries(appleii
  yaml
  z
  minizip
  )

add_executable(applen
  source/frontends/ncurses/main.cpp
  source/frontends/ncurses/world.cpp
  source/frontends/ncurses/colors.cpp
  )

target_link_libraries(applen
  appleii
  ncursesw
  )

include_directories(source)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-multichar")

MESSAGE("CMAKE_BUILD_TYPE:                ${CMAKE_BUILD_TYPE}")
MESSAGE("CMAKE_CXX_FLAGS:                 ${CMAKE_CXX_FLAGS}")
MESSAGE("CMAKE_CXX_FLAGS_RELEASE:         ${CMAKE_CXX_FLAGS_RELEASE}")
MESSAGE("CMAKE_CXX_FLAGS_DEBUG:           ${CMAKE_CXX_FLAGS_DEBUG}")
MESSAGE("CMAKE_CXX_FLAGS_RELWIHDEBINFO:   ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
