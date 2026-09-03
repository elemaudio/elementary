# Native Renderer Tests

Tests for the native `Renderer` layer under `runtime/elem/`, using GoogleTest.

## Building and running

// TODO: Improve these command instructions

```sh
# first time only, if you didn't clone with --recurse-submodules
git submodule update --init tests/third-party/googletest

cmake -S . -B build
cmake --build build --target native-renderer-tests
./build/tests/native-renderer-tests
```

You can also run a subset of tests with GoogleTest's `--gtest_filter`, e.g.:

```sh
./build/tests/native-renderer-tests --gtest_filter=NativeRendererSnapshotTests.*
```