add_executable(testcpu6502
  stdafx.cpp
  dummy.cpp
  TestCPU6502.cpp)

target_link_libraries(testcpu6502
  appleii
  )
