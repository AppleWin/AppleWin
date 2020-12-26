# there is no simple package for libretro-common
# this requires user input
set(LIBRETRO_COMMON_PATH NONE CACHE PATH "path to libretro-common")

if (EXISTS ${LIBRETRO_COMMON_PATH}/include/libretro.h)
  message("Using LIBRETRO_COMMON_PATH=${LIBRETRO_COMMON_PATH}")

  add_library(ra2 SHARED
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
    )

  target_include_directories(ra2 PRIVATE
    ${LIBRETRO_COMMON_PATH}/include
    )

  target_link_libraries(ra2 PRIVATE
    appleii
    common2
    )
else()
  message("Bad LIBRETRO_COMMON_PATH=${LIBRETRO_COMMON_PATH}, skipping libretro code")
endif()
