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

set(is_gnu "$<CXX_COMPILER_ID:GNU>")
set(before_v9 "$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9>")
set(static_stdcpp_fs "$<AND:${is_gnu},${before_v9}>")

target_link_libraries(applewin_libretro PRIVATE
  $<${static_stdcpp_fs}:stdc++fs>
  appleii
  common2
  )

if (NOT WIN32)
  target_link_libraries(applewin_libretro PUBLIC
    windows
    )
endif()

# just call it "applewin_libretro.so" as per libretro standard
set_target_properties(applewin_libretro PROPERTIES PREFIX "")

configure_file(info/applewin_libretro.info applewin_libretro.info COPYONLY)
