# not every compiler / system supports it
option(STATIC_LINKING "statically link the libretro core" OFF)

set(SOURCE_FILES
  environment.cpp
  libretro.cpp
  rdirectsound.cpp
  game.cpp
  rdirectsound.cpp
  retroregistry.cpp
  retroframe.cpp
  diskcontrol.cpp
  serialisation.cpp
  input/rkeyboard.cpp
  input/joypadbase.cpp
  input/joypad.cpp
  input/mouse.cpp
  input/analog.cpp
  input/inputremapper.cpp
  )

set(HEADER_FILES
  libretro-common/include/libretro.h
  environment.h
  rdirectsound.h
  game.h
  rdirectsound.h
  retroregistry.h
  retroframe.h
  diskcontrol.h
  serialisation.h
  input/rkeyboard.h
  input/joypadbase.h
  input/joypad.h
  input/mouse.h
  input/analog.h
  input/inputremapper.h
  )

add_library(applewin_libretro SHARED
  ${SOURCE_FILES}
  ${HEADER_FILES}
  )

target_include_directories(applewin_libretro PRIVATE
  libretro-common/include
  )

target_link_libraries(applewin_libretro PRIVATE
  appleii
  common2
  )

if (STATIC_LINKING)
  target_link_options(applewin_libretro PRIVATE
    -static-libgcc -static-libstdc++)

  target_link_libraries(applewin_libretro PRIVATE
    -Wl,-Bstatic
    ${PCAP_STATIC_LIBRARIES}
    ${SLIRP_STATIC_LIBRARIES}
    -Wl,-Bdynamic
    )
else()
  target_link_libraries(applewin_libretro PRIVATE
    ${PCAP_LIBRARIES}
    ${SLIRP_LIBRARIES}
    )
endif()

# just call it "applewin_libretro.so" as per libretro standard
set_target_properties(applewin_libretro PROPERTIES PREFIX "")

configure_file(info/applewin_libretro.info applewin_libretro.info COPYONLY)
