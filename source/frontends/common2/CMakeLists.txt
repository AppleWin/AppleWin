include(GNUInstallDirs)

set(SOURCE_FILES
  commonframe.cpp
  commoncontext.cpp
  gnuframe.cpp
  fileregistry.cpp
  ptreeregistry.cpp
  programoptions.cpp
  utils.cpp
  timer.cpp
  speed.cpp
  )

set(HEADER_FILES
  commonframe.h
  commoncontext.h
  gnuframe.h
  fileregistry.h
  ptreeregistry.h
  programoptions.h
  utils.h
  timer.h
  speed.h
  )

add_library(common2 STATIC
  ${SOURCE_FILES}
  ${HEADER_FILES}
  )

target_compile_features(common2 PUBLIC cxx_std_17)

find_package(Boost REQUIRED
  COMPONENTS program_options
  )

target_include_directories(common2 PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  ${Boost_INCLUDE_DIRS}
  )

target_link_libraries(common2 PRIVATE
  Boost::program_options
  appleii
  windows
)

file(RELATIVE_PATH ROOT_PATH ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR})
if ("${ROOT_PATH}" STREQUAL "")
  # if the 2 paths are the same
  set(ROOT_PATH "./")
endif()
file(RELATIVE_PATH SHARE_PATH ${CMAKE_INSTALL_FULL_BINDIR} ${CMAKE_INSTALL_FULL_DATADIR}/applewin)

configure_file(config.h.in config.h)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/resource
  DESTINATION ${CMAKE_INSTALL_DATADIR}/applewin)
