include(FindPkgConfig)
include(GNUInstallDirs)

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
  ${CMAKE_CURRENT_BINARY_DIR}
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
  Boost::program_options
  appleii
  )

file(RELATIVE_PATH ROOT_PATH ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR})
file(RELATIVE_PATH SHARE_PATH ${CMAKE_INSTALL_FULL_BINDIR} ${CMAKE_INSTALL_FULL_DATADIR}/applewin)

configure_file(config.h.in config.h)

install(TARGETS applen
  DESTINATION bin)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/resource
  DESTINATION ${CMAKE_INSTALL_DATADIR}/applewin)
