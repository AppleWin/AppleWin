include(FindPkgConfig)

add_library(appleii SHARED
  Speaker.cpp
  SoundCore.cpp
  AY8910.cpp
  Mockingboard.cpp
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
  Riff.cpp
  SaveState.cpp
  SynchronousEventManager.cpp
  Video.cpp
  Frame.cpp
  Core.cpp
  Utilities.cpp

  Configuration/PropertySheetHelper.cpp

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
  linux/windows/dsound.cpp
  linux/windows/guiddef.cpp
  linux/windows/dmusicc.cpp
  linux/windows/winnls.cpp

  linux/data.cpp
  linux/benchmark.cpp
  linux/paddle.cpp
  linux/version.cpp
  linux/registry.cpp
  linux/keyboard.cpp
  linux/videobuffer.cpp

  linux/duplicates/Debug.cpp
  linux/duplicates/WinVideo.cpp
  linux/duplicates/Joystick.cpp
  linux/duplicates/WinFrame.cpp
  linux/duplicates/SerialComms.cpp
  linux/duplicates/PropertySheet.cpp
  linux/duplicates/Tfe.cpp
  linux/duplicates/AppleWin.cpp

  Z80VICE/z80.cpp
  Z80VICE/z80mem.cpp
  Z80VICE/daa.cpp
  )

pkg_check_modules(YAML REQUIRED yaml-0.1)
pkg_check_modules(MINIZIP REQUIRED minizip)
find_package(Boost REQUIRED)

target_include_directories(appleii PRIVATE
  ${YAML_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  Debugger
  )

# this one appears in header files
target_include_directories(appleii PUBLIC
  ${MINIZIP_INCLUDE_DIRS}
  )

target_link_libraries(appleii PRIVATE
  ${YAML_LIBRARIES}
  ${MINIZIP_LIBRARIES}
  )

install(TARGETS appleii
  DESTINATION lib)
