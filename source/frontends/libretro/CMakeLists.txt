# there is no simple package for libretro-common
# this requires user input
set(LIBRETRO_COMMON_PATH NONE CACHE PATH "path to libretro-common")

if (EXISTS ${LIBRETRO_COMMON_PATH}/include/libretro.h)
  message("Using LIBRETRO_COMMON_PATH=${LIBRETRO_COMMON_PATH}")

  add_library(applewin_libretro SHARED
    environment.cpp
    libretro.cpp
    bitmaps.cpp
    rdirectsound.cpp
    interface.cpp
    game.cpp
    joypadbase.cpp
    joypad.cpp
    analog.cpp
    rdirectsound.cpp
    retroregistry.cpp
    )

  target_include_directories(applewin_libretro PRIVATE
    ${LIBRETRO_COMMON_PATH}/include
    )

  target_link_libraries(applewin_libretro PRIVATE
    appleii
    common2
    )

  # just call it "applewin_libretro.so" as per libretro standard
  set_target_properties(applewin_libretro PROPERTIES PREFIX "")

else()
  message("Bad LIBRETRO_COMMON_PATH=${LIBRETRO_COMMON_PATH}, skipping libretro code")
endif()
