cmake_minimum_required(VERSION 3.20)
project(native_renderer_tests VERSION 0.11.5)

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory(third-party/googletest EXCLUDE_FROM_ALL)

add_executable(native-renderer-tests
    NativeRendererTests.cpp
)

target_compile_features(native-renderer-tests PRIVATE cxx_std_17)

target_link_libraries(native-renderer-tests PRIVATE elem::runtime GTest::gtest_main GTest::gmock)

target_compile_definitions(native-renderer-tests PRIVATE
    NATIVE_RENDERER_TESTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

include(GoogleTest)
gtest_discover_tests(native-renderer-tests)
