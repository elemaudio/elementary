#pragma once

#include <ranges>
#include <vector>

#include "HashUtils.h"
#include "Types.h"
#include "Value.h"

namespace elem {
    using OutputChannel = int32_t;

    struct NodeReprShallow {
        NodeId hash;
        std::string kind;
        js::Object props;
        OutputChannel outputChannel;
    };

    /**
     * The virtual representation of an Elementary Audio Graph.
     * This represents the structure without being coupled to the implementation of
     * the audio engine, and is used to describe the desired state that the renderer
     * should realize.
     */
    struct NodeRepr : NodeReprShallow {
        std::vector<std::shared_ptr<NodeRepr>> children;

        NodeRepr(const NodeId hash, std::string kind, js::Object props, const OutputChannel outputChannel,
                          std::vector<std::shared_ptr<NodeRepr>> children)
            : NodeReprShallow{hash, std::move(kind), std::move(props), outputChannel}
              , children(std::move(children)) {
        }

        static std::shared_ptr<NodeRepr> createNode(std::string kind, js::Object props,
                                            std::vector<std::shared_ptr<NodeRepr>> children) {
            std::vector<NodeId> childHashes;
            childHashes.reserve(children.size());
            for (const auto &child: children) {
                // A node's hash must depend not just on each child's hash, but also on the
                // outputChannel being addressed on that child. Otherwise, two nodes referencing
                // different outputs of the same multi-channel child would hash identically,
                // even though they represent different signal paths.
                childHashes.push_back(HashUtils::mixNumber(child->hash, child->outputChannel));
            }

            return std::make_shared<NodeRepr>(
                HashUtils::hashNode(kind, props, childHashes),
                std::move(kind),
                std::move(props),
                0,
                std::move(children)
            );
        }

        static std::vector<std::shared_ptr<NodeRepr>> unpack(const std::shared_ptr<NodeRepr>& node, const int numChannels) {
            std::vector<std::shared_ptr<NodeRepr>> siblings;
            siblings.reserve(numChannels);

            node->outputChannel = 0;
            for (int i = 1; i < numChannels; i++) {
                auto sibling = std::make_shared<NodeRepr>(*node);
                sibling->outputChannel = i;
                siblings.push_back(sibling);
            }

            return siblings;
        }
    };
}

