include(FindPkgConfig)

add_executable(sa2
  main.cpp
  bitmaps.cpp
  ../ncurses/configuration.cpp
  ../ncurses/resources.cpp
  )

find_package(Boost REQUIRED
  COMPONENTS program_options
  )

find_package(SDL2 REQUIRED)

target_compile_features(sa2 PUBLIC cxx_std_17)

target_include_directories(sa2 PRIVATE
  ${Boost_INCLUDE_DIRS}
  ${SDL2_INCLUDE_DIRS}
  )

target_link_libraries(sa2 PRIVATE
  Boost::program_options
  ${SDL2_LIBRARIES}
  appleii
  )

if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
  target_link_libraries(sa2 PRIVATE
    stdc++fs
    )
endif()
