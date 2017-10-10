set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5Widgets)
find_package(Qt5Gamepad)

add_executable(qapple
  main.cpp
  qapple.qrc
  qapple.cpp
  qresources.cpp
  preferences.cpp
  emulator.cpp
  memorycontainer.cpp
  graphicscache.cpp
  gamepadpaddle.cpp
  video.cpp

  commands.cpp
  chunks.cpp
  qhexedit.cpp
  )

target_link_libraries(qapple
  Qt5::Widgets
  Qt5::Gamepad
  appleii
  )
