cmake_minimum_required(VERSION 3.15)
project(cli VERSION 0.11.0)

add_executable(elemcli Realtime.cpp)
add_executable(elembench Benchmark.cpp)

foreach(TARGET_NAME elemcli elembench)
  target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/choc/javascript
    ${CMAKE_SOURCE_DIR}/choc/text)

  target_compile_features(${TARGET_NAME} PRIVATE
    cxx_std_17)

  target_link_libraries(${TARGET_NAME} PRIVATE
    runtime)

  if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4)
  else()
    target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra)
  endif()
endforeach()
