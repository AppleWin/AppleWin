set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# default still Qt5
option(QAPPLE_USE_QT5 "Enable Qt5 build" OFF)

if(QAPPLE_USE_QT5)
  find_package(Qt5 REQUIRED COMPONENTS Widgets Gamepad Multimedia)
else()
  find_package(Qt6 COMPONENTS Widgets Multimedia OpenGLWidgets)
  if(NOT Qt6_FOUND)
    find_package(Qt5 REQUIRED COMPONENTS Widgets Gamepad Multimedia)
  endif()
endif()

if(Qt6_FOUND)
  message("qapple: building with Qt6")
  set(QHEXVIEW_USE_QT5 OFF)
else()
  message("qapple: building with Qt5")
  set(QHEXVIEW_USE_QT5 ON)
endif()

set(QHEXVIEW_BUILD_EXAMPLE OFF)
add_subdirectory(QHexView)

set(SOURCE_FILES
  main.cpp
  audioinfo.cpp
  qapple.cpp
  preferences.cpp
  emulator.cpp
  memorycontainer.cpp
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
  Qt::Widgets
  Qt::Multimedia
  appleii
  QHexView
  windows
  )

if(Qt5_FOUND)
  target_sources(qapple PRIVATE
    gamepadpaddle.cpp
    gamepadpaddle.h
  )

  target_link_libraries(qapple PRIVATE
    Qt::Gamepad
  )
endif()

if(Qt6_FOUND)
  target_link_libraries(qapple PRIVATE
    Qt::OpenGLWidgets
  )
endif()

install(TARGETS qapple
  DESTINATION bin)
