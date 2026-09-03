#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "elem/NodeRepr.h"

namespace elem::lib {
    struct GraphInfo {
        std::string name;
        std::string description;
        std::function<std::vector<std::shared_ptr<elem::NodeRepr>>()> build;
    };

    std::vector<GraphInfo> const& getGraphRegistry();
}
