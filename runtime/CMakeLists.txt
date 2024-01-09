cmake_minimum_required(VERSION 3.15)
project(runtime VERSION 0.11.5)

set (TargetName runtime)

add_library(${TargetName} INTERFACE)

target_include_directories(${TargetName} INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR})

add_library(elem::${TargetName} ALIAS ${TargetName})
