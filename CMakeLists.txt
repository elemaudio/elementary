cmake_minimum_required(VERSION 3.15)
project(elementary VERSION 0.11.5)

set(BENCHMARK_ENABLE_GTEST_TESTS FALSE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Adds all the module sources so they appear correctly in the IDE
set_property(GLOBAL PROPERTY USE_FOLDERS YES)

# Config options
option(JUCE_ENABLE_MODULE_SOURCE_GROUPS "Enable Module Source Groups" ON)
option(JUCE_BUILD_EXTRAS "Build JUCE Extras" OFF)

option(ONLY_BUILD_WASM "Only build the wasm subdirectory" OFF)

if(${ONLY_BUILD_WASM})
  add_subdirectory(runtime)
  add_subdirectory(wasm)
else()
  add_subdirectory(runtime)
  add_subdirectory(benchmark)
  add_subdirectory(cli2)
endif()
