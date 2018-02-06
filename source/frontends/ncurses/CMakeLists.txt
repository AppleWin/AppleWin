find_package(PkgConfig)

add_executable(applen
  main.cpp
  world.cpp
  colors.cpp
  evdevpaddle.cpp
  nframe.cpp
  asciiart.cpp
  resources.cpp
  configuration.cpp
  )

pkg_search_module(NCURSESW REQUIRED ncursesw)
pkg_search_module(LIBEVDEV REQUIRED libevdev)

target_include_directories(applen PRIVATE
  ${NCURSESW_INCLUDE_DIRS}
  ${LIBEVDEV_INCLUDE_DIRS}
  )

target_link_libraries(applen
  boost_program_options
  appleii
  ${NCURSESW_LIBRARIES}
  ${LIBEVDEV_LIBRARIES}
  )

target_compile_options(applen PRIVATE
  ${NCURSESW_CFLAGS_OTHER}
  ${LIBEVDEV_CFLAGS_OTHER}
  )
