#include "HelloSine.h"

#include "elem/lib/Math.h"
#include "elem/lib/Oscillators.h"

namespace elem::lib {
    std::vector<std::shared_ptr<elem::NodeRepr>> buildHelloSineGraph() {
        return {
            mul({0.3, cycle(440.0)}),
            mul({0.3, cycle(440.0)}),
        };
    }
}
