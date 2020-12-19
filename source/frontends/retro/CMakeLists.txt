# there is no simple package for libretro-common
# this requires user input
set(LIBRETRO_PATH NONE CACHE PATH "path to libretro-common")

if (IS_DIRECTORY ${LIBRETRO_PATH})
  message("Using LIBRETRO_PATH=${LIBRETRO_PATH}")

  add_library(ra2 SHARED
    environment.cpp
    libretro.cpp
    bitmaps.cpp
    rdirectsound.cpp
    interface.cpp
    game.cpp
    joypad.cpp
    )

  target_include_directories(ra2 PRIVATE
    ${LIBRETRO_PATH}/include
    )

  target_link_libraries(ra2 PRIVATE
    appleii
    common2
    )
else()
  message("Bad LIBRETRO_PATH=${LIBRETRO_PATH}, skipping libretro code")
endif()
