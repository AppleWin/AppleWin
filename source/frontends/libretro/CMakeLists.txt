# not every compiler / system supports it
option(STATIC_LINKING "statically link the libretro core" OFF)

set(SOURCE_FILES
  environment.cpp
  libretro.cpp
  rdirectsound.cpp
  game.cpp
  joypadbase.cpp
  joypad.cpp
  mouse.cpp
  analog.cpp
  rkeyboard.cpp
  rdirectsound.cpp
  retroregistry.cpp
  retroframe.cpp
  diskcontrol.cpp
  serialisation.cpp
  )

set(HEADER_FILES
  libretro-common/include/libretro.h
  environment.h
  rdirectsound.h
  game.h
  joypadbase.h
  joypad.h
  mouse.h
  analog.h
  rkeyboard.h
  rdirectsound.h
  retroregistry.h
  retroframe.h
  diskcontrol.h
  serialisation.h
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

  if (NOT WIN32)
    # on Ubuntu libz cannot be statically linked to generate a .so
    # it has not been compiled with -fPIC
    target_link_libraries(applewin_libretro PRIVATE
      -Wl,-Bstatic
      ${MINIZIP_LIBRARIES}
      ${YAML_STATIC_LIBRARIES}
      ${PCAP_STATIC_LIBRARIES}
      ${SLIRP_STATIC_LIBRARIES}
      -Wl,-Bdynamic
      ${ZLIB_LIBRARIES}
      )
  else()
    target_link_libraries(applewin_libretro PRIVATE
      -Wl,-Bstatic
      ${MINIZIP_STATIC_LIBRARIES}
      ${YAML_STATIC_LIBRARIES}
      ${PCAP_STATIC_LIBRARIES}
      ${SLIRP_STATIC_LIBRARIES}
      ${ZLIB_STATIC_LIBRARIES}
      -Wl,-Bdynamic
      )
  endif()
else()
  target_link_libraries(applewin_libretro PRIVATE
    ${MINIZIP_LIBRARIES}
    ${YAML_LIBRARIES}
    ${PCAP_LIBRARIES}
    ${SLIRP_LIBRARIES}
    ${ZLIB_LIBRARIES}
    )
endif()

if (NOT WIN32)
  target_link_libraries(applewin_libretro PUBLIC
    windows
    )
endif()

# just call it "applewin_libretro.so" as per libretro standard
set_target_properties(applewin_libretro PROPERTIES PREFIX "")

configure_file(info/applewin_libretro.info applewin_libretro.info COPYONLY)
