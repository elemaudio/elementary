#pragma once

#include <memory>
#include <vector>

#include "elem/NodeRepr.h"

namespace elem::lib {
    std::vector<std::shared_ptr<elem::NodeRepr>> buildHelloSineGraph();
}
