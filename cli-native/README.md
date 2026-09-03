# cli-native

This version of the cli uses the native renderer instead of the js based renderer to render the Audio Graph.

The cli tool here is not meant to be a fully featured, end-user product, but rather
a concise demonstration of using Elementary's native renderer with the native engine.

## Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target elemcli-native
```

## Running

- `./build/elemcli-native --list` lists the available graphs.
- `./build/elemcli-native --graph <name>` plays the named graph to the
  default audio device. Press Enter to exit.
- `./build/elemcli-native --help` prints usage information.

## Adding a graph

1. Add `graphs/MyGraph.h`/`.cpp` exposing a `buildMyGraph()` function that
   returns a `std::vector<std::shared_ptr<elem::SymbolicGraphNode>>` (see
   `graphs/HelloSine.h`/`.cpp` for an example).
2. Register it by adding an entry to the vector in
   `graphs/GraphRegistry.cpp`.
3. Add `graphs/MyGraph.cpp` to the `elemcli_native_graph` sources in
   `CMakeLists.txt`.
