add_library(ra2 SHARED
  ~/projects/games/retroarch/libretro-super/retroarch/libretro-common/formats/bmp/rbmp.c
  environment.cpp
  libretro.cpp
  bitmaps.cpp
  rdirectsound.cpp
  )

target_include_directories(ra2 PRIVATE
  ~/projects/games/retroarch/libretro-super/retroarch/libretro-common
  ~/projects/games/retroarch/libretro-super/retroarch/libretro-common/include
  )

target_link_libraries(ra2 PRIVATE
  appleii
  common2
  )
