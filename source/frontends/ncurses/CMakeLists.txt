include(FindPkgConfig)

add_executable(applen
  main.cpp
  world.cpp
  colors.cpp
  evdevpaddle.cpp
  nframe.cpp
  asciiart.cpp
  resources.cpp
  configuration.cpp
  bitmaps.cpp
  )

pkg_search_module(NCURSESW REQUIRED ncursesw)
pkg_search_module(LIBEVDEV REQUIRED libevdev)
find_package(Boost REQUIRED
  COMPONENTS program_options
  )

target_include_directories(applen PRIVATE
  ${NCURSESW_INCLUDE_DIRS}
  ${LIBEVDEV_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  )

target_compile_options(applen PRIVATE
  ${NCURSESW_CFLAGS_OTHER}
  ${LIBEVDEV_CFLAGS_OTHER}
  )
target_compile_features(applen PUBLIC cxx_std_17)

target_link_libraries(applen PRIVATE
  ${NCURSESW_LIBRARIES}
  ${LIBEVDEV_LIBRARIES}
  Boost::program_options
  appleii
  )

if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
  target_link_libraries(applen PRIVATE
    stdc++fs
    )
endif()

add_custom_target(copy-resources ALL
    COMMAND cmake -E copy_directory ${CMAKE_SOURCE_DIR}/resource ${CMAKE_BINARY_DIR}/resource)
