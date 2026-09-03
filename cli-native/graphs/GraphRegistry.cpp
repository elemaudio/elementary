#include "GraphRegistry.h"

#include "HelloSine.h"

namespace elem::lib {
    std::vector<GraphInfo> const& getGraphRegistry() {
        static const std::vector<GraphInfo> registry = {
            {"hello-sine", "A single 440Hz sine tone at 0.3 gain", buildHelloSineGraph},
        };

        return registry;
    }
}
