#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "RendererBenchmarkScenario.h"
#include <elem/Runtime.h>

namespace benchmark {
    std::pair<GraphBuildFn, GraphRenderFn> makeRenderSineNoKeyBenchmark(
        const std::shared_ptr<elem::Runtime<float>>& runtime);

    using NativeScenarioFactory = std::function<std::pair<GraphBuildFn, GraphRenderFn>(
        const std::shared_ptr<elem::Runtime<float>>&)>;

    // Names every native renderer scenario so the CLI can look one up by name
    // (or list them) without a hardcoded switch statement at the call site.
    const std::map<std::string, NativeScenarioFactory>& nativeRendererScenarios();
}
