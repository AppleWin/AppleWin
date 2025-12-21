include(GNUInstallDirs)

set(SOURCE_FILES
  argparser.cpp
  commonframe.cpp
  commoncontext.cpp
  controllerdoublepress.cpp
  gnuframe.cpp
  fileregistry.cpp
  ptreeregistry.cpp
  programoptions.cpp
  utils.cpp
  timer.cpp
  speed.cpp
  yamlmap.cpp
  yamlselftest.cpp
  )

set(HEADER_FILES
  argparser.h
  commonframe.h
  commoncontext.h
  controllerdoublepress.h
  gnuframe.h
  fileregistry.h
  ptreeregistry.h
  programoptions.h
  utils.h
  timer.h
  speed.h
  yamlmap.h
  )

add_library(common2 STATIC
  ${SOURCE_FILES}
  ${HEADER_FILES}
  )

target_include_directories(common2 PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  )

target_link_libraries(common2 PRIVATE
  appleii
  apple2roms
  )

file(RELATIVE_PATH ROOT_PATH ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR})
if ("${ROOT_PATH}" STREQUAL "")
  # if the 2 paths are the same
  set(ROOT_PATH "./")
endif()

# yaml parsing is so complicated that we need a test!
add_executable(testyaml
  yamlmap.cpp
  yamlselftest.cpp
  )

target_link_libraries(testyaml PRIVATE
  yaml
  )

configure_file(common_config.h.in common_config.h)
