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
option(ONLY_BUILD_SDK "Only build the sdk subdirectory" OFF)
option(ELEM_BUILD_LITE "Build with restricted functionality" OFF)

# Projects
if(DEFINED ENV{CI})
  # When running on CI we build a subset of projects for now to keep costs down
  add_subdirectory(runtime)
  add_subdirectory(juce)

  if (DEFINED ENV{BUILD_CLI})
    add_subdirectory(platform)
    add_subdirectory(cli)
  endif()
  if (DEFINED ENV{BUILD_PLUGIN})
    add_subdirectory(plugin/next)
  endif()
  if (DEFINED ENV{BUILD_SDK})
    add_subdirectory(platform)
    add_subdirectory(sdk)
  endif()
elseif(${ONLY_BUILD_WASM})
  # We distinguish the WASM build here to avoid pulling in juce or node below, which
  # is needed for just about everything else. This way we can easily run the wasm
  # build inside a docker container without worrying about extra dependencies
  add_subdirectory(runtime)
  add_subdirectory(wasm)
elseif(${ONLY_BUILD_SDK})
  add_subdirectory(runtime)
  add_subdirectory(sdk)
else()
  add_subdirectory(runtime)
  add_subdirectory(juce)
  add_subdirectory(plugin/next)
  add_subdirectory(benchmark)
  add_subdirectory(cli2)
endif()
