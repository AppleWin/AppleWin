include(FindPkgConfig)

find_package(SDL2 REQUIRED)
pkg_search_module(SDL2_IMAGE REQUIRED SDL2_image)
pkg_search_module(GLES2 REQUIRED glesv2)
set(IMGUI_PATH "imgui/imgui")

add_executable(sa2)

set(SOURCE_FILES
  main.cpp
  gamepad.cpp
  sdirectsound.cpp
  utils.cpp
  sdlframe.cpp
  renderer/sdlrendererframe.cpp
  )

set(HEADER_FILES
  gamepad.h
  sdirectsound.h
  utils.h
  sdlframe.h
  renderer/sdlrendererframe.h
  )


target_compile_features(sa2 PUBLIC cxx_std_17)

target_include_directories(sa2 PRIVATE
  ${SDL2_INCLUDE_DIRS}
  ${SDL2_IMAGE_INCLUDE_DIRS}
  )

target_link_libraries(sa2 PRIVATE
  ${SDL2_LIBRARIES}
  ${SDL2_IMAGE_LIBRARIES}
  ${GLES2_LIBRARIES}
  appleii
  common2
  )

target_sources(sa2 PRIVATE
  ${SOURCE_FILES}
  ${HEADER_FILES}
  )

target_sources(sa2 PRIVATE
  imgui/sdlimguiframe.cpp
  imgui/image.cpp
  imgui/settingshelper.cpp

  imgui/sdlimguiframe.h
  imgui/image.h
  imgui/settingshelper.h
  imgui/imconfig.h
  imgui/gles.h

  ${IMGUI_PATH}/imgui.h
  ${IMGUI_PATH}/imgui.cpp
  ${IMGUI_PATH}/imgui_demo.cpp
  ${IMGUI_PATH}/imgui_draw.cpp
  ${IMGUI_PATH}/imgui_tables.cpp
  ${IMGUI_PATH}/imgui_widgets.cpp
  ${IMGUI_PATH}/backends/imgui_impl_sdl.cpp
  ${IMGUI_PATH}/backends/imgui_impl_opengl3.cpp
  )

target_include_directories(sa2 PRIVATE
  ${IMGUI_PATH}
  ${IMGUI_PATH}/backends
  )

target_compile_definitions(sa2 PRIVATE
  IMGUI_IMPL_OPENGL_ES2
  IMGUI_USER_CONFIG="frontends/sdl/imgui/imconfig.h"
  )

install(TARGETS sa2
  DESTINATION bin)
