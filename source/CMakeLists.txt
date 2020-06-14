include(FindPkgConfig)

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
  LanguageCard.cpp
  RGBMonitor.cpp
  NTSC.cpp
  NTSC_CharSet.cpp
  CardManager.cpp
  Disk2CardManager.cpp

  linux/windows/memory.cpp
  linux/windows/handles.cpp
  linux/windows/files.cpp
  linux/windows/resources.cpp
  linux/windows/bitmap.cpp
  linux/windows/time.cpp
  linux/windows/stringcb.cpp
  linux/windows/strings.cpp
  linux/windows/misc.cpp
  linux/windows/winbase.cpp
  linux/windows/winuser.cpp

  linux/data.cpp
  linux/dummies.cpp
  linux/state.cpp
  linux/benchmark.cpp
  linux/paddle.cpp
  linux/version.cpp
  linux/registry.cpp
  linux/keyboard.cpp

  linux/duplicates/Debug.cpp
  linux/duplicates/Video.cpp
  linux/duplicates/Mockingboard.cpp
  linux/duplicates/Joystick.cpp
  linux/duplicates/Frame.cpp
  linux/duplicates/SerialComms.cpp
  linux/duplicates/Applewin.cpp

  Z80VICE/z80.cpp
  Z80VICE/z80mem.cpp
  Z80VICE/daa.cpp
  )

pkg_check_modules(YAML REQUIRED yaml-0.1)
pkg_check_modules(MINIZIP REQUIRED minizip)
find_package(Boost REQUIRED)

target_include_directories(appleii PRIVATE
  ${YAML_INCLUDE_DIRS}
  ${MINIZIP_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  Debugger
  )

target_link_libraries(appleii PRIVATE
  ${YAML_LIBRARIES}
  ${MINIZIP_LIBRARIES}
  )
