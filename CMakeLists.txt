cmake_minimum_required(VERSION 3.15)
project(elementary VERSION 0.11.5)

option(ONLY_BUILD_WASM "Only build the wasm subdirectory" OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

if(MSVC)
  # MSVC's default (non-conformant) preprocessor mishandles __VA_ARGS__
  # expansion in nested macros, which breaks the ELEM_ARG_COUNT/DEFINE_PROPS_STRUCT
  # macros in runtime/elem/lib/Props.h. The conformant preprocessor fixes this.
  add_compile_options(/Zc:preprocessor)
endif()

if(${ONLY_BUILD_WASM})
  add_subdirectory(runtime)
  add_subdirectory(wasm)
else()
  enable_testing()
  add_subdirectory(runtime)
  add_subdirectory(cli)
  add_subdirectory(cli-native)
  add_subdirectory(tests)
endif()
