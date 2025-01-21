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
  )

if (STATIC_LINKING)
  target_link_options(applen PRIVATE
    -static-libgcc -static-libstdc++)

  target_link_libraries(applen PRIVATE
    -Wl,-Bstatic
    ${YAML_STATIC_LIBRARIES}
    ${MINIZIP_STATIC_LIBRARIES}
    ${PCAP_STATIC_LIBRARIES}
    ${SLIRP_STATIC_LIBRARIES}
    ${ZLIB_STATIC_LIBRARIES}
    ${NCURSES_STATIC_LIBRARIES}
    ${LIBEVDEV_STATIC_LIBRARIES}
    -Wl,-Bdynamic
    )
else()
  target_link_libraries(applen PRIVATE
    ${YAML_LIBRARIES}
    ${MINIZIP_LIBRARIES}
    ${PCAP_LIBRARIES}
    ${SLIRP_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${NCURSES_LIBRARIES}
    ${LIBEVDEV_LIBRARIES}
    )
endif()

target_link_directories(applen PRIVATE
  ${NCURSES_LIBRARY_DIRS}
  ${LIBEVDEV_LIBRARY_DIRS}
  )

install(TARGETS applen
  DESTINATION bin)
