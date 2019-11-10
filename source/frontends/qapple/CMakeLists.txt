set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

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
  settings.cpp
  configuration.cpp
  audiogenerator.cpp
  loggingcategory.cpp

  commands.cpp
  chunks.cpp
  qhexedit.cpp
  )

find_package(Qt5 REQUIRED
  COMPONENTS Widgets Gamepad Multimedia
  )

target_link_libraries(qapple
  Qt5::Widgets
  Qt5::Gamepad
  Qt5::Multimedia
  appleii
  )
