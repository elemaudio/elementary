cmake_minimum_required(VERSION 3.15)
project(wasm VERSION 0.11.0)

set(TargetName elementary-wasm)
set(CMAKE_VERBOSE_MAKEFILE ON)

add_executable(${TargetName}
  Main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/FFTConvolver/TwoStageFFTConvolver.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/FFTConvolver/FFTConvolver.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/FFTConvolver/AudioFFT.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/FFTConvolver/Utilities.cpp)

target_include_directories(${TargetName} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/FFTConvolver)

target_compile_features(${TargetName} PRIVATE
  cxx_std_17)

target_link_libraries(${TargetName} PRIVATE
  elem::runtime)

set(PRE "${CMAKE_CURRENT_SOURCE_DIR}/pre.js")

set(EM_FLAGS
  "--pre-js ${PRE}"
  "-lembind"
  "-s WASM=1"
  "-s WASM_ASYNC_COMPILATION=1"
  "-s EXPORT_ES6=1"
  "-s MODULARIZE=1"
  "-s ENVIRONMENT=web,webview,worker,node"
  "-s ALLOW_MEMORY_GROWTH=1"
)

list(JOIN EM_FLAGS " " EM_FLAGS)

set_target_properties(${TargetName}
  PROPERTIES
  COMPILE_FLAGS "-O3"
  LINK_FLAGS ${EM_FLAGS})
