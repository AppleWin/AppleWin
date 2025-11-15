include(FindPkgConfig)
include(FindCurses)

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
  ncurses_safe.h
  )

add_executable(applen
  ${SOURCE_FILES}
  ${HEADER_FILES}
  )

set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)  # required for Unicode in GR and HGR
find_package(Curses REQUIRED)

if (NOT WIN32)
  pkg_search_module(LIBEVDEV REQUIRED libevdev)
endif()

target_include_directories(applen PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  ${NCURSES_INCLUDE_DIRS}
  ${LIBEVDEV_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  )

target_compile_options(applen PRIVATE
  ${NCURSES_CFLAGS}
  ${LIBEVDEV_CFLAGS_OTHER}
  )

target_link_libraries(applen PRIVATE
  appleii
  common2
  minizip
  yaml

  ${PCAP_LIBRARIES}
  ${SLIRP_LIBRARIES}
  
  ${NCURSES_LIBRARIES}
  ${LIBEVDEV_LIBRARIES}
  )

target_link_directories(applen PRIVATE
  ${NCURSES_LIBRARY_DIRS}
  ${LIBEVDEV_LIBRARY_DIRS}
  )

configure_file(applen_config.h.in applen_config.h)

install(TARGETS applen
  DESTINATION bin)
