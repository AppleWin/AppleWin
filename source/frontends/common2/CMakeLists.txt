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
  )

add_library(common2 STATIC
  ${SOURCE_FILES}
  ${HEADER_FILES}
  )

find_package(Boost REQUIRED)

target_include_directories(common2 PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  ${Boost_INCLUDE_DIRS}
  )

target_link_libraries(common2 PRIVATE
  appleii
  apple2roms
)

if (NOT WIN32)
  target_link_libraries(common2 PUBLIC
    windows
    )
endif()

file(RELATIVE_PATH ROOT_PATH ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR})
if ("${ROOT_PATH}" STREQUAL "")
  # if the 2 paths are the same
  set(ROOT_PATH "./")
endif()

configure_file(config.h.in config.h)
