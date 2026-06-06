cmake_minimum_required(VERSION 3.15)
project(runtime VERSION 0.11.5)

set (TargetName runtime)

add_library(${TargetName} INTERFACE)

target_include_directories(${TargetName} INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR})

add_library(elem::${TargetName} ALIAS ${TargetName})

if(BUILD_TESTING AND NOT ONLY_BUILD_WASM)
  add_executable(runtime_tests tests/PhasorNodeTest.cpp)
  target_link_libraries(runtime_tests PRIVATE elem::${TargetName})
  target_compile_features(runtime_tests PRIVATE cxx_std_17)
  add_test(NAME runtime_tests COMMAND runtime_tests)
endif()
