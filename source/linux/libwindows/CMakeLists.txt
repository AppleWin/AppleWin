set(SOURCE_FILES
  dmusicc.cpp
  dsound.cpp
  fileapi.cpp
  guiddef.cpp
  winhandles.cpp
  misc.cpp
  strsafe.cpp
  winstrings.cpp
  timeapi.cpp
  winbase.cpp
  winnls.cpp
  winuser.cpp
  )

set(HEADER_FILES
  dmusicc.h
  dsound.h
  fileapi.h
  gdi.h
  guiddef.h
  winhandles.h
  misc.h
  mmreg.h
  mmsystem.h
  strsafe.h
  winstrings.h
  timeapi.h
  wingdi.h
  winbase.h
  wincompat.h
  windows.h
  winerror.h
  winnls.h
  winuser.h
  joystickapi.h
  )

add_library(windows STATIC
  ${SOURCE_FILES}
  ${HEADER_FILES}
  )

target_include_directories(windows PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  )
