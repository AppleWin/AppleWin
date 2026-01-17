include(FindPkgConfig)

if (NOT WIN32)
  pkg_search_module(SLIRP slirp)

  # if slirp is not found, we will try pcap
  # if it is not found either, a dummy pcap will be compiled
  # but network will be badly affected

  # pcacp on Windows in managed in a different way, so we disable it
  if ("${SLIRP_FOUND}" STREQUAL "")
    message(WARNING "'libslirp' not found: will try 'libpcap' instead")

    pkg_search_module(PCAP libpcap)
    if ("${PCAP_FOUND}" STREQUAL "")
      # old versions of pcap do not work with pkg-config
      # this is necessary on Rapsberri Pi OS
      execute_process(COMMAND pcap-config --cflags
        OUTPUT_VARIABLE PCAP_INCLUDE_DIRS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE STATUS)
      if ("${STATUS}" STREQUAL "0")
        message("Found 'libpcap' via pcap-config")
        execute_process(COMMAND pcap-config --libs
          OUTPUT_VARIABLE PCAP_LIBRARIES
          OUTPUT_STRIP_TRAILING_WHITESPACE)
        set(PCAP_FOUND "1")
      endif()
    endif()
  endif()
endif()

set(SOURCE_FILES
  Tfe/tfesupp.cpp
  Tfe/NetworkBackend.cpp
  Tfe/PCapBackend.cpp
  Tfe/IPRaw.cpp
  Tfe/DNS.cpp

  Debugger/BreakpointCard.cpp
  Debugger/Debug.cpp
  Debugger/Debugger_Help.cpp
  Debugger/Debugger_Color.cpp
  Debugger/Debugger_Disassembler.cpp
  Debugger/Debugger_Symbols.cpp
  Debugger/Debugger_DisassemblerData.cpp
  Debugger/Debugger_Console.cpp
  Debugger/Debugger_Assembler.cpp
  Debugger/Debugger_Parser.cpp
  Debugger/Debugger_Range.cpp
  Debugger/Debugger_Commands.cpp
  Debugger/Util_MemoryTextFile.cpp

  CopyProtectionDongles.cpp
  Uthernet1.cpp
  Uthernet2.cpp
  StrFormat.cpp
  6522.cpp
  VidHD.cpp
  SSI263.cpp
  Speaker.cpp
  SoundCore.cpp
  AY8910.cpp
  Mockingboard.cpp
  MockingboardCardManager.cpp
  Pravets.cpp
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
  ProDOS_Utils.cpp
  MouseInterface.cpp
  LanguageCard.cpp
  RGBMonitor.cpp
  NTSC.cpp
  NTSC_CharSet.cpp
  Card.cpp
  CardManager.cpp
  Disk2CardManager.cpp
  Riff.cpp
  SaveState.cpp
  SynchronousEventManager.cpp
  Video.cpp
  Core.cpp
  Utilities.cpp
  FrameBase.cpp
  CmdLine.cpp

  Configuration/PropertySheetHelper.cpp
  Configuration/Config.cpp

  linux/bitmap.cpp
  linux/benchmark.cpp
  linux/paddle.cpp
  linux/version.cpp
  linux/registryclass.cpp
  linux/linuxframe.cpp
  linux/linuxsoundbuffer.cpp
  linux/context.cpp
  linux/cassettetape.cpp
  linux/network/slirp2.cpp
  linux/network/portfwds.cpp

  linux/duplicates/Debugger_Display.cpp
  linux/duplicates/Debugger_Win32.cpp
  linux/duplicates/Joystick.cpp
  linux/duplicates/SerialComms.cpp
  linux/duplicates/PropertySheet.cpp
  linux/duplicates/Registry.cpp
  linux/duplicates/FourPlay.cpp
  linux/duplicates/SNESMAX.cpp
  linux/duplicates/Keyboard.cpp
  linux/duplicates/PageConfig.cpp
  linux/duplicates/PageConfigTfe.cpp
  linux/duplicates/PageInput.cpp
  linux/duplicates/PageSound.cpp
  linux/duplicates/PageDisk.cpp
  linux/duplicates/PageAdvanced.cpp

  Z80VICE/z80.cpp
  Z80VICE/z80mem.cpp
  Z80VICE/daa.cpp
  )

set(HEADER_FILES
  Tfe/tfearch.h
  Tfe/tfesupp.h
  Tfe/NetworkBackend.h
  Tfe/PCapBackend.h
  Tfe/IPRaw.h
  Tfe/DNS.h

  CopyProtectionDongles.h
  Uthernet1.h
  Uthernet2.h
  W5100.h
  6522.h
  VidHD.h
  SSI263.h
  SSI263Phonemes.h
  Speaker.h
  SoundCore.h
  AY8910.h
  Mockingboard.h
  MockingboardCardManager.h
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
  MemoryDefs.h
  CPU.h
  6821.h
  NoSlotClock.h
  SAM.h
  z80emu.h
  ParallelPrinter.h
  ProDOS_Utils.h
  ProDOS_FileSystem.h
  MouseInterface.h
  LanguageCard.h
  RGBMonitor.h
  NTSC.h
  NTSC_CharSet.h
  Card.h
  CardManager.h
  Disk2CardManager.h
  Riff.h
  SaveState.h
  SynchronousEventManager.h
  Video.h
  Core.h
  Utilities.h
  FrameBase.h
  FourPlay.h
  SNESMAX.h
  SoundBuffer.h

  Common.h
  DiskDefs.h
  DiskLog.h
  Interface.h

  Debugger/BreakpointCard.h
  Debugger/Debug.h
  Debugger/Debugger_Color.h
  Debugger/Debugger_Console.h
  Debugger/Debugger_Disassembler.h
  Debugger/Debugger_DisassemblerData.h
  Debugger/Debugger_Display.h
  Debugger/Debugger_Help.h
  Debugger/Debugger_Parser.h
  Debugger/Debugger_Range.h
  Debugger/Debugger_Symbols.h
  Debugger/Debugger_Types.h
  Debugger/Debugger_Win32.h
  Debugger/Util_MemoryTextFile.h
  Debugger/Util_Text.h

  Configuration/PropertySheetHelper.h
  Configuration/Config.h

  linux/bitmap.h
  linux/benchmark.h
  linux/paddle.h
  linux/version.h
  linux/registryclass.h
  linux/keyboardbuffer.h
  linux/linuxframe.h
  linux/linuxsoundbuffer.h
  linux/cassettetape.h
  linux/network/slirp2.h
  linux/network/portfwds.h

  Z80VICE/z80.h
  Z80VICE/z80mem.h
  Z80VICE/z80regs.h
  Z80VICE/daa.h
  )

# we used to generate a shared object, but it turns out there are more cons than pros
add_library(appleii STATIC
  ${SOURCE_FILES}
  ${HEADER_FILES}
  )

if ("${PCAP_FOUND}" STREQUAL "1")
  target_sources(appleii PRIVATE Tfe/tfearch.cpp)
else()
  target_sources(appleii PRIVATE linux/duplicates/tfearch.cpp)
endif()

target_include_directories(appleii PRIVATE
  ${PCAP_INCLUDE_DIRS}
  ${SLIRP_INCLUDE_DIRS}
  Debugger
  )

# this one appears in header files
target_include_directories(appleii PUBLIC
  ${CMAKE_CURRENT_BINARY_DIR}  # for config.h
  )

if (WIN32)
  target_link_libraries(appleii PUBLIC
    wsock32
    iphlpapi
    winmm
    strmiids
    )
else()
  target_link_libraries(appleii PUBLIC
    windows
    )
endif()

target_link_libraries(appleii PUBLIC
  yaml
  minizip
  )

target_link_directories(appleii PUBLIC
  ${PCAP_LIBRARY_DIRS}
  ${SLIRP_LIBRARY_DIRS}
  )

target_compile_options(appleii PUBLIC
  -Wno-multichar
  )

# wildcards (*.SYM) are not supported on Windows msys2
add_custom_command(
  TARGET appleii POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/bin/APPLE2E.SYM ${CMAKE_SOURCE_DIR}/bin/A2_BASIC.SYM ${CMAKE_SOURCE_DIR}/bin/A2_DOS33.SYM2 ${CMAKE_BINARY_DIR}
  )

configure_file(linux/linux_config.h.in linux/linux_config.h)
