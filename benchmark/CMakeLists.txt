cmake_minimum_required(VERSION 3.15)
project(benchmark VERSION 0.11.0)

set(TARGET_NAME benchmark)

add_executable(${TARGET_NAME} Main.cpp)

target_include_directories(${TARGET_NAME} PRIVATE
  ${CMAKE_SOURCE_DIR}/choc/text)

target_compile_features(${TARGET_NAME} PRIVATE
  cxx_std_17)

target_link_libraries(${TARGET_NAME} PRIVATE
  runtime)
