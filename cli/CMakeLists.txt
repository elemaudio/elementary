cmake_minimum_required(VERSION 3.15)
project(cli VERSION 0.11.0)

add_library(elemcli_core STATIC Realtime.cpp Benchmark.cpp)

target_include_directories(elemcli_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/choc/javascript
  ${CMAKE_CURRENT_SOURCE_DIR}/choc/text)

target_compile_features(elemcli_core PUBLIC
  cxx_std_17)

target_link_libraries(elemcli_core PUBLIC
  elem::runtime)

if(MSVC)
  target_compile_options(elemcli_core PRIVATE /W4)
else()
  target_compile_options(elemcli_core PRIVATE -Wall -Wextra)
endif()

add_executable(elemcli RealtimeMain.cpp)
add_executable(elembench BenchmarkMain.cpp)

target_link_libraries(elemcli PRIVATE elemcli_core)
target_link_libraries(elembench PRIVATE elemcli_core)

if(UNIX AND NOT APPLE)
  find_package(Threads REQUIRED)
  target_link_libraries(elemcli PRIVATE
    Threads::Threads
    ${CMAKE_DL_LIBS})
endif()

