#pragma once

#include <functional>

#include <elem/Runtime.h>


/*
 * Your main can call this function to run a complete benchmark test for either
 * float or double processing. Before the benchmark starts, your initCallback
 * will be called with a reference to the runtime for additional initialization,
 * like adding a custom node type or filling the shared resource map.
 */
template <typename FloatType>
void runBenchmark(std::string const& name, std::string const& inputFileName, std::function<void(elem::Runtime<FloatType>&)>&& initCallback);
