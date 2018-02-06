add_custom_command(OUTPUT linux/version.hpp
  COMMAND grep FILEVERSION ${CMAKE_CURRENT_SOURCE_DIR}/../resource/Applewin.rc | sed 's/FILEVERSION/\#define FILEVERSION/' > linux/version.hpp
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../resource/Applewin.rc
)

add_library(appleii SHARED
  SaveState.cpp
  Pravets.cpp
  Tape.cpp
  YamlHelper.cpp
  Log.cpp
  Disk.cpp
  DiskFormatTrack.cpp
  DiskImage.cpp
  DiskImageHelper.cpp
  Harddisk.cpp
  Memory.cpp
  CPU.cpp
  6821.cpp
  NoSlotClock.cpp
  SAM.cpp
  z80emu.cpp
  ParallelPrinter.cpp
  MouseInterface.cpp

  linux/data.cpp
  linux/dummies.cpp
  linux/wwrapper.cpp
  linux/state.cpp
  linux/benchmark.cpp
  linux/paddle.cpp
  linux/version.cpp
  linux/version.hpp

  Z80VICE/z80.cpp
  Z80VICE/z80mem.cpp
  Z80VICE/daa.cpp
  )

target_link_libraries(appleii
  yaml
  z
  minizip
  )
