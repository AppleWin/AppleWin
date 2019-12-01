cmake_minimum_required(VERSION 3.9)

project(applewin)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-multichar -Werror=return-type")

MESSAGE("CMAKE_BUILD_TYPE:                ${CMAKE_BUILD_TYPE}")
MESSAGE("CMAKE_CXX_FLAGS:                 ${CMAKE_CXX_FLAGS}")
MESSAGE("CMAKE_CXX_FLAGS_RELEASE:         ${CMAKE_CXX_FLAGS_RELEASE}")
MESSAGE("CMAKE_CXX_FLAGS_DEBUG:           ${CMAKE_CXX_FLAGS_DEBUG}")
MESSAGE("CMAKE_CXX_FLAGS_RELWIHDEBINFO:   ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(CheckIPOSupported)
check_ipo_supported()
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

include_directories(source)

add_subdirectory(source)
add_subdirectory(source/frontends/ncurses)
add_subdirectory(source/frontends/qapple)
add_subdirectory(test/TestCPU6502)
