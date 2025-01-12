add_executable(testcpu6502
  stdafx.cpp
  ../../source/SynchronousEventManager.cpp
  TestCPU6502.cpp)

if (NOT WIN32)
  target_link_libraries(testcpu6502
    windows)
endif()
