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

# We use these flags for compiling our wasm bundle such that the exported Module
# factory function asynchronously initializes the module. It's important for the browser
# and node.js contexts, whereas we want synchronous initialization for the webaudio worklet
set(EM_FLAGS_ASYNC "--pre-js ${PRE} -lembind --closure 1 -s WASM=1 -s WASM_ASYNC_COMPILATION=1 -s MODULARIZE=1 -s ENVIRONMENT=shell -s SINGLE_FILE=1 -s ALLOW_MEMORY_GROWTH=1")

# We use these flags for compiling our wasm bundle such that the exported Module
# factory function _synchronously_ initializes the module. We use this specifically in the
# initialization of the web audio worklet where we don't necessarily have room to wait for
# an asynchronous init
set(EM_FLAGS_SYNC "--pre-js ${PRE} -lembind --closure 1 -s WASM=1 -s WASM_ASYNC_COMPILATION=0 -s MODULARIZE=1 -s ENVIRONMENT=shell -s SINGLE_FILE=1 -s ALLOW_MEMORY_GROWTH=1")

if ($ENV{ELEM_BUILD_ASYNC})
  set_target_properties(${TargetName}
    PROPERTIES
    COMPILE_FLAGS "-O3"
    LINK_FLAGS ${EM_FLAGS_ASYNC})
else()
  set_target_properties(${TargetName}
    PROPERTIES
    COMPILE_FLAGS "-O3"
    LINK_FLAGS ${EM_FLAGS_SYNC})
endif()
