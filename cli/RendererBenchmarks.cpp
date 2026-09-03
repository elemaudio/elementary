#include "RendererBenchmarks.h"

#include "elem/lib/Oscillators.h"

namespace benchmark {
    std::pair<GraphBuildFn, GraphRenderFn> makeRenderSineNoKeyBenchmark(
        const std::shared_ptr<elem::Runtime<float>>& runtime) {

        return makeNativeGraphFns(runtime, [](const size_t i) {
            return elem::lib::cycle(440.0 + i);
        });
    }

    const std::map<std::string, NativeScenarioFactory>& nativeRendererScenarios() {
        static const std::map<std::string, NativeScenarioFactory> scenarios = {
            {"sine_no_key", &makeRenderSineNoKeyBenchmark},
        };
        return scenarios;
    }
}
