#pragma once

#include <algorithm>
#include <chrono>
#include <string>
#include <unordered_set>

#include "Runtime.h"
#include "NodeRepr.h"

namespace elem {
    namespace JsInstructionType {
        static constexpr js::Number CREATE_NODE = static_cast<js::Number>(RuntimeInstructionType::CREATE_NODE);
        static constexpr js::Number APPEND_CHILD = static_cast<js::Number>(RuntimeInstructionType::APPEND_CHILD);
        static constexpr js::Number SET_PROPERTY = static_cast<js::Number>(RuntimeInstructionType::SET_PROPERTY);
        static constexpr js::Number ACTIVATE_ROOTS = static_cast<js::Number>(RuntimeInstructionType::ACTIVATE_ROOTS);
        static constexpr js::Number COMMIT_UPDATES = static_cast<js::Number>(RuntimeInstructionType::COMMIT_UPDATES);
    }

    struct InstructionBatch {
        js::Array createNode;
        js::Array appendChild;
        js::Array setProperty;
        js::Array activateRoots;
        js::Array commitUpdates;

        [[nodiscard]] js::Array takeBatchedInstructions() {
            js::Array instructions;
            instructions.reserve(
                createNode.size()
                + appendChild.size()
                + setProperty.size()
                + activateRoots.size()
                + commitUpdates.size()
            );
            for (auto &v: createNode) {
                instructions.push_back(std::move(v));
            }
            for (auto &v: appendChild) {
                instructions.push_back(std::move(v));
            }
            for (auto &v: setProperty) {
                instructions.push_back(std::move(v));
            }
            for (auto &v: activateRoots) {
                instructions.push_back(std::move(v));
            }
            for (auto &v: commitUpdates) {
                instructions.push_back(std::move(v));
            }
            return std::move(instructions);
        }
    };

    struct RenderOptions {
        int32_t fadeInMs = 20;
        int32_t fadeOutMs = 20;
    };

    struct RenderResult {
        int result = 0;
        int32_t nodesAdded = 0;
        int32_t edgesAdded = 0;
        int32_t propsWritten = 0;
        double elapsedTimeMs = 0.0;
    };

    struct NodeRef {
        std::shared_ptr<NodeRepr> node;
        std::function<RenderResult(js::Object newProps)> setter;
    };

    template<typename FloatType>
    class Renderer {
    public:
        explicit Renderer(std::shared_ptr<Runtime<FloatType> > runtime);

        RenderResult renderGraph(std::vector<std::shared_ptr<NodeRepr>> graphs, RenderOptions options = {});

        NodeRef createRef(std::string kind, js::Object props, std::vector<std::shared_ptr<NodeRepr>> children);

    private:
        static js::Array makeCreateNodeInstruction(std::string kind, NodeId hash);

        static js::Array makeAppendChildInstruction(NodeId parentHash, NodeId childHash,
                                                    OutputChannel childOutputChannel);

        static js::Array makeSetPropertyInstruction(NodeId hash, std::string key, js::Value value);

        static js::Array makeActivateRootsInstruction(std::set<NodeId> roots);

        static js::Array makeCommitUpdatesInstruction();

        static void updateNodeProps(NodeId hash, const js::Object& oldProps, const js::Object& newProps, InstructionBatch &batch);

        void mount(const NodeRepr &node, InstructionBatch &batch);

        std::shared_ptr<Runtime<FloatType> > mRuntime;
        NodeId nextRefId = 0;
    };

    template<typename FloatType>
    Renderer<FloatType>::Renderer(std::shared_ptr<Runtime<FloatType> > runtime) : mRuntime{std::move(runtime)} {
    }

    template<typename FloatType>
    void Renderer<FloatType>::updateNodeProps(const NodeId hash, const js::Object& oldProps, const js::Object& newProps, InstructionBatch &batch) {
        for (const auto& [key, value] : newProps) {
            if (auto oldProp = oldProps.find(key); oldProp == oldProps.end() || oldProp->second != value) {
                batch.setProperty.emplace_back(makeSetPropertyInstruction(hash, key, value));
            }
        }
    }

    template<typename FloatType>
    void Renderer<FloatType>::mount(const NodeRepr &node, InstructionBatch &batch) {
        if (const auto *existingNode = mRuntime->findNode(node.hash); existingNode != nullptr) {
            updateNodeProps(node.hash, existingNode->getProperties(), node.props, batch);
        } else {
            batch.createNode.emplace_back(makeCreateNodeInstruction(node.kind, node.hash));
            for (const auto &[key, value]: node.props) {
                batch.setProperty.emplace_back(makeSetPropertyInstruction(node.hash, key, value));
            }
            for (const auto &child: node.children) {
                batch.appendChild.emplace_back(makeAppendChildInstruction(node.hash, child->hash, child->outputChannel));
            }
        }
    }

    template<typename FloatType>
    RenderResult Renderer<FloatType>::renderGraph(std::vector<std::shared_ptr<NodeRepr>> graphs, const RenderOptions options) {
        auto const t0 = std::chrono::steady_clock::now();

        std::unordered_set<NodeId> visited;
        InstructionBatch instructions;

        // Wrap the roots of the graph in root-type nodes
        std::vector<std::shared_ptr<NodeRepr>> roots;
        roots.reserve(graphs.size());
        for (int i = 0; i < graphs.size(); ++i) {
            std::vector<std::shared_ptr<NodeRepr>> children;
            children.push_back(std::move(graphs[i]));
            roots.push_back(
                NodeRepr::createNode(
                    "root",
                    {
                        {"channel", static_cast<js::Number>(i)},
                        {"fadeInMs", static_cast<js::Number>(options.fadeInMs)},
                        {"fadeOutMs", static_cast<js::Number>(options.fadeOutMs)},
                    },
                    std::move(children)
                )
            );
        }

        std::vector<std::shared_ptr<NodeRepr>> stack;
        for (const auto &root: roots) {
            stack.push_back(root);
        }

        while (!stack.empty()) {
            const auto node = stack.back();
            stack.pop_back();

            if (const auto &found = visited.find(node->hash);
                found != visited.end()) { continue; }
            visited.insert(node->hash);

            mount(*node, instructions);

            for (const auto &child: node->children) {
                stack.push_back(child);
            }
        }

        std::set<NodeId> rootHashes;
        for (const auto &root: roots) {
            rootHashes.insert(root->hash);
        }

        if (rootHashes != mRuntime->getCurrentRoots()) {
            instructions.activateRoots.emplace_back(makeActivateRootsInstruction(
                    std::move(rootHashes)
                )
            );
        }
        instructions.commitUpdates.emplace_back(makeCommitUpdatesInstruction());

        RenderResult stats;
        stats.nodesAdded = static_cast<int32_t>(instructions.createNode.size());
        stats.edgesAdded = static_cast<int32_t>(instructions.appendChild.size());
        stats.propsWritten = static_cast<int32_t>(instructions.setProperty.size());

        auto const t1 = std::chrono::steady_clock::now();
        stats.elapsedTimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

        stats.result = mRuntime->applyInstructions(instructions.takeBatchedInstructions());

        return stats;
    }

    template<typename FloatType>
    NodeRef Renderer<FloatType>::createRef(std::string kind, js::Object props, std::vector<std::shared_ptr<NodeRepr>> children) {
        props["key"] = "__refKey:" + std::to_string(nextRefId++);
        auto node = NodeRepr::createNode(std::move(kind), std::move(props), std::move(children));

        std::weak_ptr<Runtime<FloatType>> wRuntime = mRuntime;

        auto setProperty = [hash = node->hash, wRuntime](const js::Object& newProps) -> RenderResult {
            RenderResult stats;

            const auto& runtime = wRuntime.lock();
            if (runtime == nullptr) {
                stats.result = ReturnCode::RuntimeExpired();
                return stats;
            }

            const auto* existing = runtime->findNode(hash);
            if (existing == nullptr) {
                stats.result = ReturnCode::NodeNotFound();
                return stats;
            }

            InstructionBatch instructions;
            updateNodeProps(hash, existing->getProperties(), newProps, instructions);
            stats.propsWritten = static_cast<int32_t>(instructions.setProperty.size());

            instructions.commitUpdates.emplace_back(makeCommitUpdatesInstruction());
            stats.result = runtime->applyInstructions(instructions.takeBatchedInstructions());

            return stats;
        };

        return NodeRef{
            std::move(node),
            std::move(setProperty)
        };
    }

    template<typename FloatType>
    js::Array Renderer<FloatType>::makeCreateNodeInstruction(std::string kind, const NodeId hash) {
        return {JsInstructionType::CREATE_NODE, static_cast<js::Number>(hash), js::Value(std::move(kind))};
    }

    template<typename FloatType>
    js::Array Renderer<FloatType>::makeAppendChildInstruction(const NodeId parentHash, const NodeId childHash,
                                                              const OutputChannel childOutputChannel) {
        return {
            JsInstructionType::APPEND_CHILD, static_cast<js::Number>(parentHash),
            static_cast<js::Number>(childHash), static_cast<js::Number>(childOutputChannel)
        };
    }

    template<typename FloatType>
    js::Array Renderer<FloatType>::makeSetPropertyInstruction(const NodeId hash, std::string key, js::Value value) {
        return {
            JsInstructionType::SET_PROPERTY, static_cast<js::Number>(hash),
            js::Value(std::move(key)), std::move(value)
        };
    }

    template<typename FloatType>
    js::Array Renderer<FloatType>::makeActivateRootsInstruction(std::set<NodeId> roots) {
        return {JsInstructionType::ACTIVATE_ROOTS, js::Array(roots.begin(), roots.end())};
    }

    template<typename FloatType>
    js::Array Renderer<FloatType>::makeCommitUpdatesInstruction() {
        return {JsInstructionType::COMMIT_UPDATES};
    }
}
