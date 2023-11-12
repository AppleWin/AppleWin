set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED
  COMPONENTS Widgets Gamepad Multimedia
  )

add_subdirectory(QHexView)

set(SOURCE_FILES
  main.cpp
  audioinfo.cpp
  qapple.cpp
  preferences.cpp
  emulator.cpp
  memorycontainer.cpp
  gamepadpaddle.cpp
  qvideo.cpp
  configuration.cpp
  options.cpp
  loggingcategory.cpp
  viewbuffer.cpp
  qdirectsound.cpp
  qtframe.cpp
  )

set(HEADER_FILES
  applicationname.h
  audioinfo.h
  qapple.h
  preferences.h
  emulator.h
  memorycontainer.h
  gamepadpaddle.h
  qvideo.h
  configuration.h
  options.h
  loggingcategory.h
  viewbuffer.h
  qdirectsound.h
  qtframe.h
  )

add_executable(qapple
  ${SOURCE_FILES}
  ${HEADER_FILES}
  qapple.qrc
  )

target_link_libraries(qapple PRIVATE
  Qt5::Widgets
  Qt5::Gamepad
  Qt5::Multimedia
  appleii
  qhexview-lib
  windows
  )

install(TARGETS qapple
  DESTINATION bin)
