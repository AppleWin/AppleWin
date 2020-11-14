include(GNUInstallDirs)

add_library(common2 STATIC
  resources.cpp
  configuration.cpp
  programoptions.cpp
  utils.cpp
  timer.cpp
  )

find_package(Boost REQUIRED
  COMPONENTS program_options
  )

target_include_directories(common2 PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  ${Boost_INCLUDE_DIRS}
  )

target_link_libraries(common2 PRIVATE
  Boost::program_options
)

file(RELATIVE_PATH ROOT_PATH ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR})
file(RELATIVE_PATH SHARE_PATH ${CMAKE_INSTALL_FULL_BINDIR} ${CMAKE_INSTALL_FULL_DATADIR}/applewin)

configure_file(config.h.in config.h)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/resource
  DESTINATION ${CMAKE_INSTALL_DATADIR}/applewin)
