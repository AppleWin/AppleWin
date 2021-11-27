set(SOURCE_FILES
  environment.cpp
  libretro.cpp
  rdirectsound.cpp
  game.cpp
  joypadbase.cpp
  joypad.cpp
  mouse.cpp
  analog.cpp
  rdirectsound.cpp
  retroregistry.cpp
  retroframe.cpp
  diskcontrol.cpp
  )

set(HEADER_FILES
  environment.h
  rdirectsound.h
  game.h
  joypadbase.h
  joypad.h
  mouse.h
  analog.h
  rdirectsound.h
  retroregistry.h
  retroframe.h
  diskcontrol.h
  libretro-common/include/libretro.h
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

# just call it "applewin_libretro.so" as per libretro standard
set_target_properties(applewin_libretro PROPERTIES PREFIX "")
