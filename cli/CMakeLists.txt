cmake_minimum_required(VERSION 3.15)
project(cli VERSION 0.11.0)

add_library(elemcli_core STATIC Realtime.cpp)
target_include_directories(elemcli_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(elemcli CLIMain.cpp)
target_link_libraries(elemcli PRIVATE elemcli_core)

add_executable(elembench Benchmark.cpp)

foreach(TARGET_NAME elemcli_core elembench)
  target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/choc/javascript
    ${CMAKE_CURRENT_SOURCE_DIR}/choc/text)

  target_compile_features(${TARGET_NAME} PRIVATE
    cxx_std_17)

  target_link_libraries(${TARGET_NAME} PUBLIC
    runtime)

  if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4)
  else()
    target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra)
  endif()
endforeach()
