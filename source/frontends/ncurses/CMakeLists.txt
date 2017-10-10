include_directories(/usr/include/libevdev-1.0)

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

target_link_libraries(applen
  boost_program_options
  appleii
  ncursesw
  evdev
  )
