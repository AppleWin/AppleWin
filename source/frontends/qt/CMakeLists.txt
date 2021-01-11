set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED
  COMPONENTS Widgets Gamepad Multimedia
  )

add_subdirectory(QHexView)

add_executable(qapple
  main.cpp
  qapple.qrc
  qapple.cpp
  qresources.cpp
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
)

target_link_libraries(qapple PRIVATE
  Qt5::Widgets
  Qt5::Gamepad
  Qt5::Multimedia
  appleii
  qhexview-lib
  )

install(TARGETS qapple
  DESTINATION bin)
