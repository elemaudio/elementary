#pragma once
#include <variant>

#include "../NodeRepr.h"
#include "../Value.h"

namespace elem::lib {
    using NodeReprSPtr = std::shared_ptr<NodeRepr>;
    using ElemNode = std::variant<NodeReprSPtr, js::Number>;

    static NodeReprSPtr constant(const js::Number value, std::optional<std::string> key=std::nullopt) {
        js::Object props;
        props.insert({"value", value});
        if (key.has_value()) {
            props.insert({"key", std::move(*key)});
        }
        return NodeRepr::createNode("const", std::move(props), {});
    }

    /**
     * Some nodes can be represented by literal values. For example, constants can
     * be written as numerical values. The `resolve` method wraps theses literal
     * representations with the correct node.
     */
    static NodeReprSPtr resolve(ElemNode repr) {
        return std::visit([](auto&& r) {
           using T = std::decay_t<decltype(r)>;
           if constexpr (std::is_same_v<T, double>) {
               return constant(std::forward<decltype(r)>(r));
           } else if constexpr (std::is_same_v<T, NodeReprSPtr>) {
               return std::forward<decltype(r)>(r);
           }
       }, repr);
    }

    static std::vector<NodeReprSPtr> resolve(std::vector<ElemNode> xs) {
        std::vector<NodeReprSPtr> res;
        res.reserve(xs.size());
        for (auto& x : xs) {
            res.emplace_back(resolve(std::move(x)));
        }
        return res;
    }

    /**
     * Utility function for addressing multiple output channels from a given graph node.
     */
    static std::vector<NodeReprSPtr> unpack(const NodeReprSPtr& node, const int numChannels) {
        std::vector<NodeReprSPtr> result;
        result.reserve(numChannels);

        for (int i = 0; i < numChannels; i++) {
            auto copy = std::make_shared<NodeRepr>(*node);
            copy->outputChannel = i;
            result.push_back(std::move(copy));
        }

        return result;
    }
}
