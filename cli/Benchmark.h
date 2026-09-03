#pragma once

#include <functional>

#include <elem/Runtime.h>

inline const auto* kConsoleShimScript = R"script(
(function() {
  if (typeof globalThis.console === 'undefined') {
    globalThis.console = {
      log(...args) {
        return __log__('[log]', ...args);
      },
      warn(...args) {
        return __log__('[warn]', ...args);
      },
      error(...args) {
        return __log__('[error]', ...args);
      },
    };
  }
})();
)script";

/*
 * Your main can call this function to run a complete benchmark test for either
 * float or double processing. Before the benchmark starts, your initCallback
 * will be called with a reference to the runtime for additional initialization,
 * like adding a custom node type or filling the shared resource map.
 */
template <typename FloatType>
void runBenchmark(std::string const& name, std::string const& inputFileName, std::function<void(elem::Runtime<FloatType>&)>&& initCallback);