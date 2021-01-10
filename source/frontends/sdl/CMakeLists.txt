include(FindPkgConfig)

set(SOURCE_FILES
  main.cpp
  bitmaps.cpp
  emulator.cpp
  gamepad.cpp
  sdirectsound.cpp
  utils.cpp
  sdlframe.cpp
  )

set(HEADER_FILES
  emulator.h
  gamepad.h
  sdirectsound.h
  utils.h
  sdlframe.h
  )

add_executable(sa2
  ${SOURCE_FILES}
  ${HEADER_FILES}
  )

find_package(SDL2 REQUIRED)
pkg_search_module(SDL2_IMAGE REQUIRED SDL2_image)

target_compile_features(sa2 PUBLIC cxx_std_17)

target_include_directories(sa2 PRIVATE
  ${SDL2_INCLUDE_DIRS}
  ${SDL2_IMAGE_INCLUDE_DIRS}
  )

target_link_libraries(sa2 PRIVATE
  ${SDL2_LIBRARIES}
  ${SDL2_IMAGE_LIBRARIES}
  appleii
  common2
  )

install(TARGETS sa2
  DESTINATION bin)
