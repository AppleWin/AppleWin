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

pkg_search_module(NCURSES REQUIRED ncursesw)

target_include_directories(applen PRIVATE
  /usr/include/libevdev-1.0
  ${NCURSESW_INCLUDE_DIRS}
  )

target_link_libraries(applen
  boost_program_options
  appleii
  ncursesw
  evdev
  ${NCURSESW_LIBRARIES}
  )

target_compile_options(applen PRIVATE
  ${NCURSESW_CFLAGS_OTHER}
  )
