include(FindPkgConfig)

set(SOURCE_FILES
  main.cpp
  world.cpp
  colors.cpp
  evdevpaddle.cpp
  nframe.cpp
  asciiart.cpp
  )

set(HEADER_FILES
  world.h
  colors.h
  evdevpaddle.h
  nframe.h
  asciiart.h
  )

add_executable(applen
  ${SOURCE_FILES}
  ${HEADER_FILES}
  )

pkg_search_module(NCURSESW REQUIRED ncursesw)
pkg_search_module(LIBEVDEV REQUIRED libevdev)

target_include_directories(applen PRIVATE
  ${NCURSESW_INCLUDE_DIRS}
  ${LIBEVDEV_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  )

target_compile_options(applen PRIVATE
  ${NCURSESW_CFLAGS_OTHER}
  ${LIBEVDEV_CFLAGS_OTHER}
  )

target_link_libraries(applen PRIVATE
  ${NCURSESW_LIBRARIES}
  ${LIBEVDEV_LIBRARIES}
  appleii
  common2
  )

target_link_directories(applen PRIVATE
  ${NCURSESW_LIBRARY_DIRS}
  ${LIBEVDEV_LIBRARY_DIRS}
  )

install(TARGETS applen
  DESTINATION bin)
