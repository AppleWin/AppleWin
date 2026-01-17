include(FindPkgConfig)
include(FindOpenGL)

add_executable(sa2)

set(IMGUI_PATH "imgui/imgui")
set(IMGUI_CLUB_PATH "imgui/imgui_club")

option(SA2_SDL3 "Use SDL3" OFF)

if (SA2_SDL3)
  message("sa2: using SDL3")
  find_package(SDL3 REQUIRED)
  find_package(SDL3_image REQUIRED)

  target_link_libraries(sa2 PRIVATE
    SDL3::SDL3
    SDL3_image::SDL3_image
  )

  target_sources(sa2 PRIVATE
    ${IMGUI_PATH}/backends/imgui_impl_sdl3.cpp
  )

  target_compile_definitions(sa2 PRIVATE SDL_ENABLE_OLD_NAMES)
else()
  message("sa2: using SDL2")
  find_package(SDL2 REQUIRED)
  # we should use find_package, but Ubuntu does not provide it for SDL2_image
  pkg_search_module(SDL2_IMAGE REQUIRED SDL2_image)

  target_include_directories(sa2 PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
  )

  target_link_libraries(sa2 PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
  )

  target_link_directories(sa2 PRIVATE
    ${SDL2_LIBRARY_DIRS}
    ${SDL2_IMAGE_LIBRARY_DIRS}
  )

  target_sources(sa2 PRIVATE
    ${IMGUI_PATH}/backends/imgui_impl_sdl2.cpp
  )
endif()

if ((${CMAKE_SYSTEM_NAME} MATCHES "Darwin") OR (${CMAKE_SYSTEM_NAME} MATCHES "Windows"))
  # only OpenGL supported on MacOS and Windows
  set (SA2_OPENGL ON)
else()
  option(SA2_OPENGL "Prefer OpenGL over OpenGL ES" OFF)
endif()

if (SA2_OPENGL)
  find_package(OpenGL REQUIRED)
else()
  pkg_search_module(GLES2 REQUIRED glesv2)
  target_compile_definitions(sa2 PRIVATE
    IMGUI_IMPL_OPENGL_ES2
  )
endif()


set(SOURCE_FILES
  main.cpp
  gamepad.cpp
  sdirectsound.cpp
  utils.cpp
  sdlframe.cpp
  processfile.cpp
  sdlcompat.cpp
  renderer/sdlrendererframe.cpp
  )

set(HEADER_FILES
  gamepad.h
  sdirectsound.h
  utils.h
  sdlframe.h
  processfile.h
  sdlcompat.h
  renderer/sdlrendererframe.h
  )

target_include_directories(sa2 PRIVATE
  ${GLES2_INCLUDE_DIRS}
  )

target_link_libraries(sa2 PRIVATE
  appleii
  common2

  ${PCAP_LIBRARIES}
  ${SLIRP_LIBRARIES}

  ${GLES2_LIBRARIES}
  OpenGL::GL
  )

target_link_directories(sa2 PRIVATE
  ${GLES2_LIBRARY_DIRS}
  )

target_sources(sa2 PRIVATE
  ${SOURCE_FILES}
  ${HEADER_FILES}
  )

target_sources(sa2 PRIVATE
  imgui/sdlimguiframe.cpp
  imgui/image.cpp
  imgui/settingshelper.cpp
  imgui/sdlsettings.cpp
  imgui/sdldebugger.cpp
  imgui/sdlmemory.cpp
  imgui/inputtexthistory.cpp
  imgui/cycletabitems.cpp

  imgui/sdlimguiframe.h
  imgui/image.h
  imgui/settingshelper.h
  imgui/sdlsettings.h
  imgui/sdldebugger.h
  imgui/sdlmemory.h
  imgui/sa2_imconfig.h
  imgui/glselector.h
  imgui/inputtexthistory.h
  imgui/cycletabitems.h

  ${IMGUI_PATH}/imgui.h
  ${IMGUI_PATH}/imgui.cpp
  ${IMGUI_PATH}/imgui_demo.cpp
  ${IMGUI_PATH}/imgui_draw.cpp
  ${IMGUI_PATH}/imgui_tables.cpp
  ${IMGUI_PATH}/imgui_widgets.cpp
  ${IMGUI_PATH}/backends/imgui_impl_opengl3.cpp

  ${IMGUI_CLUB_PATH}/imgui_memory_editor/imgui_memory_editor.h
  )

target_include_directories(sa2 PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}  # for config.h
  ${IMGUI_PATH}
  ${IMGUI_PATH}/backends
  ${IMGUI_CLUB_PATH}/imgui_memory_editor
  )

target_compile_definitions(sa2 PRIVATE
  IMGUI_USER_CONFIG="frontends/sdl/imgui/sa2_imconfig.h"
  )

configure_file(sa2_config.h.in sa2_config.h)

install(TARGETS sa2
  DESTINATION bin)
