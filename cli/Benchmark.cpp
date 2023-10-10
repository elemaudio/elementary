#include <chrono>
#include <thread>
#include <vector>
#include <iostream>

#include <choc_Files.h>
#include <choc_javascript.h>
#include <choc_javascript_QuickJS.h>

#include "Benchmark.h"


const auto* kConsoleShimScript = R"script(
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

template <typename FloatType>
void runBenchmark(std::string const& name, std::string const& inputFileName, std::function<void(elem::Runtime<FloatType>&)>&& initCallback) {
    elem::Runtime<FloatType> runtime(44100.0, 512);

    // Allow additional user initialization
    initCallback(runtime);

    auto ctx = choc::javascript::createQuickJSContext();

    ctx.registerFunction("__postNativeMessage__", [&](choc::javascript::ArgumentList args) {
        runtime.applyInstructions(elem::js::parseJSON(args[0]->toString()));
        return choc::value::Value();
    });

    ctx.registerFunction("__log__", [](choc::javascript::ArgumentList args) {
        for (size_t i = 0; i < args.numArgs; ++i) {
            std::cout << choc::json::toString(*args[i], true) << std::endl;
        }

        return choc::value::Value();
    });

    // Shim the js environment for console logging
    (void) ctx.evaluate(kConsoleShimScript);

    // Execute the input file
    auto inputFile = choc::file::loadFileAsString(inputFileName);
    auto rv = ctx.evaluate(inputFile);

    // Now we benchmark the realtime processing step
    std::vector<std::vector<FloatType>> scratchBuffers;
    std::vector<FloatType*> scratchPointers;

    for (int i = 0; i < 2; ++i) {
        scratchBuffers.push_back(std::vector<FloatType>(512));
        scratchPointers.push_back(scratchBuffers[i].data());
    }

    // Run the first block to process the events
    runtime.process(
        nullptr,
        0,
        scratchPointers.data(),
        2,
        512,
        0
    );

    // Now we can measure the static render process. We have this sleep
    // here to clearly demarcate, in the profiling timeline, which work is
    // related to the event processing above and which work is related to this
    // work loop here
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::vector<double> deltas;

    for (size_t i = 0; i < 10'000; ++i) {
        auto t0 = std::chrono::steady_clock::now();

        runtime.process(
            nullptr,
            0,
            scratchPointers.data(),
            2,
            512,
            0
        );

        auto t1 = std::chrono::steady_clock::now();
        auto diffms = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        deltas.push_back(diffms);
    }

    // Reporting
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto const sum = std::accumulate(deltas.begin(), deltas.end(), 0.0);
    auto const avg = sum / (double) deltas.size();

    std::cout << "[Running " << name << "]:" << std::endl;
    std::cout << "Total run time: " << sum << "us " << "(" << (sum / 1000000) << "s)" << std::endl;
    std::cout << "Average iteration time: " << avg << "us" << std::endl;
    std::cout << "Done" << std::endl << std::endl;
}

template void runBenchmark<float>(std::string const& name, std::string const& inputFileName, std::function<void(elem::Runtime<float>&)>&& initCallback);
template void runBenchmark<double>(std::string const& name, std::string const& inputFileName, std::function<void(elem::Runtime<double>&)>&& initCallback);
