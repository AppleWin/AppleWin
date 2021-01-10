include(FindPkgConfig)

set(SOURCE_FILES
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
  Core.cpp
  Utilities.cpp
  FrameBase.cpp

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
  linux/linuxframe.cpp
  linux/context.cpp

  linux/duplicates/Debug.cpp
  linux/duplicates/Joystick.cpp
  linux/duplicates/SerialComms.cpp
  linux/duplicates/PropertySheet.cpp
  linux/duplicates/Tfe.cpp

  Z80VICE/z80.cpp
  Z80VICE/z80mem.cpp
  Z80VICE/daa.cpp
  )

set(HEADER_FILES
  Speaker.h
  SoundCore.h
  AY8910.h
  Mockingboard.h
  Pravets.h
  Tape.h
  YamlHelper.h
  Log.h
  Disk.h
  DiskFormatTrack.h
  DiskImage.h
  DiskImageHelper.h
  Harddisk.h
  Memory.h
  CPU.h
  6821.h
  NoSlotClock.h
  SAM.h
  z80emu.h
  ParallelPrinter.h
  MouseInterface.h
  LanguageCard.h
  RGBMonitor.h
  NTSC.h
  NTSC_CharSet.h
  CardManager.h
  Disk2CardManager.h
  Riff.h
  SaveState.h
  SynchronousEventManager.h
  Video.h
  Core.h
  Utilities.h
  FrameBase.h

  Common.h
  DiskDefs.h
  DiskLog.h
  Interface.h
  SaveState_Structs_common.h
  SaveState_Structs_v1.h

  Configuration/PropertySheetHelper.h

  linux/windows/handles.h
  linux/windows/files.h
  linux/windows/resources.h
  linux/windows/bitmap.h
  linux/windows/time.h
  linux/windows/stringcb.h
  linux/windows/strings.h
  linux/windows/misc.h
  linux/windows/winbase.h
  linux/windows/winuser.h
  linux/windows/dsound.h
  linux/windows/guiddef.h
  linux/windows/dmusicc.h
  linux/windows/winnls.h
  linux/windows/gdi.h
  linux/windows/mmreg.h
  linux/windows/mmsystem.h

  linux/interface.h
  linux/data.h
  linux/benchmark.h
  linux/paddle.h
  linux/version.h
  linux/registry.h
  linux/keyboard.h
  linux/linuxframe.h
  linux/win.h

  Z80VICE/z80.h
  Z80VICE/z80mem.h
  Z80VICE/z80regs.h
  Z80VICE/daa.h
  )

add_library(appleii SHARED
  ${SOURCE_FILES}
  ${HEADER_FILES}
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
