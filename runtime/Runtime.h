#pragma once

#include <set>
#include <unordered_map>

#include "builtins/helpers/RefCountedPool.h"
#include "DefaultNodeTypes.h"
#include "GraphNode.h"
#include "GraphRenderSequence.h"
#include "Invariant.h"
#include "Types.h"
#include "Value.h"
#include "JSON.h"


#ifndef ELEM_DBG
  #ifdef NDEBUG
    #define ELEM_DBG(x)
  #else
    #include <iostream>
    #define ELEM_DBG(x) std::cout << "[Debug]" << x << std::endl;
  #endif
#endif


namespace elem
{

    // The Runtime is the primary interface for embedding the Elementary engine within
    // your project, independent of the JavaScript engine.
    //
    // A Runtime instance manages the processing graph, coordinates mutations against
    // that graph, and runs the realtime processing loop. Once the Runtime instance is
    // in place, it can receive instructions from the JavaScript frontend, wherever that
    // frontend is running.
    //
    // Users may also implement custom graph nodes by extending the GraphNode
    // interface and then registering with the Runtime via `registerNodeType`.
    template <typename FloatType>
    class Runtime
    {
    public:
        //==============================================================================
        Runtime(double sampleRate, int blockSize);

        //==============================================================================
        // Apply graph rendering instructions
        void applyInstructions(js::Array const& batch);

        // Run the internal audio processing callback
        void process(
            const FloatType** inputChannelData,
            int numInputChannels,
            FloatType** outputChannelData,
            int numOutputChannels,
            int numSamples,
            int64_t sampleTime);

        //==============================================================================
        // Process queued events
        //
        // This raises events from the processing graph such as new data from analysis nodes
        // or errors encountered while processing.
        void processQueuedEvents(std::function<void(std::string const&, js::Value)> evtCallback);

        // Reset the internal graph nodes
        //
        // This allows each node to optionally reset any internal state, such as
        // delay buffers or sample readers.
        void reset();

        //==============================================================================
        // Loads a shared buffer into memory.
        //
        // This method populates an internal map from which any GraphNode can request a
        // shared pointer to the data.
        void updateSharedResourceMap(std::string const& name, FloatType const* data, size_t size);

        // For registering custom GraphNode factory functions.
        //
        // New node types must inherit GraphNode, and the factory function must produce a shared
        // pointer to a new node instance.
        //
        // After registering your node type, the runtime instance will be ready to receive
        // instructions for your new type, such as those produced by the frontend
        // from, e.g., `core.render(createNode("myNewNodeType", props, [children]))`
        using NodeFactoryFn = std::function<std::shared_ptr<GraphNode<FloatType>>(GraphNodeId const id, double sampleRate, int const blockSize)>;
        void registerNodeType (std::string const& type, NodeFactoryFn && fn);

    private:
        //==============================================================================
        // The rendering interface
        enum class InstructionType {
          CREATE_NODE = 0,
          DELETE_NODE = 1,
          APPEND_CHILD = 2,
          SET_PROPERTY = 3,
          ACTIVATE_ROOTS = 4,
          COMMIT_UPDATES = 5,
        };

        void createNode(std::string const& hash, std::string const& type);
        void deleteNode(std::string const& hash);
        void setProperty(std::string const& hash, std::string const& prop, js::Value const& v);
        void appendChild(std::string const& parentHash, std::string const& childHash);
        void activateRoots(js::Array const& v);

        BufferAllocator<FloatType> bufferAllocator;
        std::shared_ptr<GraphRenderSequence<FloatType>> rtRenderSeq;
        std::shared_ptr<GraphRenderSequence<FloatType>> buildRenderSequence();
        void traverse(std::set<GraphNodeId>& visited, std::vector<GraphNodeId>& visitOrder, GraphNodeId const& n);

        SingleWriterSingleReaderQueue<std::shared_ptr<GraphRenderSequence<FloatType>>> rseqQueue;

        //==============================================================================
        std::unordered_map<std::string, NodeFactoryFn> nodeFactory;
        std::unordered_map<int64_t, std::shared_ptr<GraphNode<FloatType>>> nodeTable;
        std::unordered_map<int64_t, std::vector<int64_t>> edgeTable;
        std::unordered_map<int64_t, std::shared_ptr<GraphNode<FloatType>>> garbageTable;

        std::set<GraphNodeId> currentRoots;
        RefCountedPool<GraphRenderSequence<FloatType>> renderSeqPool;

        SharedResourceMap<FloatType> sharedResourceMap;

        double sampleRate;
        int blockSize;
    };

    //==============================================================================
    // Details...
    template <typename FloatType>
    Runtime<FloatType>::Runtime(double sampleRate, int blockSize)
        : bufferAllocator(blockSize)
        , sampleRate(sampleRate)
        , blockSize(blockSize)
    {
        DefaultNodeTypes<FloatType>::forEach([this](std::string const& type, NodeFactoryFn&& fn) {
            registerNodeType(type, std::move(fn));
        });
    }

    //==============================================================================
    template <typename FloatType>
    void Runtime<FloatType>::applyInstructions(elem::js::Array const& batch)
    {
        for (auto& next : batch) {
            invariant(next.isArray(), "Expected a command array.");
            auto const& ar = next.getArray();

            invariant(ar[0].isNumber(), "Expected a number type command.");
            auto const cmd = static_cast<InstructionType>(static_cast<int>((elem::js::Number) ar[0]));

            switch (cmd) {
                case InstructionType::CREATE_NODE:
                    createNode((elem::js::String) ar[1], (elem::js::String) ar[2]);
                    break;
                case InstructionType::DELETE_NODE:
                    deleteNode((elem::js::String) ar[1]);
                    break;
                case InstructionType::SET_PROPERTY:
                    setProperty((elem::js::String) ar[1], (elem::js::String) ar[2], ar[3]);
                    break;
                case InstructionType::APPEND_CHILD:
                    appendChild((elem::js::String) ar[1], (elem::js::String) ar[2]);
                    break;
                case InstructionType::ACTIVATE_ROOTS:
                    activateRoots(ar[1].getArray());
                    break;
                case InstructionType::COMMIT_UPDATES:
                    rseqQueue.push(buildRenderSequence());
                    break;
                default:
                    break;
            }
        }

        // While we're here, we scan the garbageTable to see if we can deallocate
        // any nodes who are only held by the garbageTable. That means that the realtime
        // thread has already seen the corresponding DeleteNode events and dropped its references
        for (auto it = garbageTable.begin(); it != garbageTable.end();) {
            if (it->second.use_count() == 1) {
                ELEM_DBG("[Native] pruneNode " << graphNodeIdToString(it->second->getId()));
                it = garbageTable.erase(it);
            } else {
                it++;
            }
        }
    }

    template <typename FloatType>
    void Runtime<FloatType>::process(const FloatType** inputChannelData, int numInputChannels, FloatType** outputChannelData, int numOutputChannels, int numSamples, int64_t sampleTime)
    {
        while (rseqQueue.size() > 0) {
            rseqQueue.pop(rtRenderSeq);
        }

        if (rtRenderSeq) {
            rtRenderSeq->process(inputChannelData, numInputChannels, outputChannelData, numOutputChannels, numSamples, sampleTime);
        }
    }

    //==============================================================================
    template <typename FloatType>
    void Runtime<FloatType>::createNode(std::string const& hash, std::string const& type)
    {
        ELEM_DBG("[Native] createNode " << type << "#" << hash);

        int64_t nodeId = std::stoll(hash, nullptr, 16);

        invariant(nodeFactory.find(type) != nodeFactory.end(), "Unknown node type " + type);
        invariant(nodeTable.find(nodeId) == nodeTable.end(), "Trying to create a node which already exists.");
        invariant(edgeTable.find(nodeId) == edgeTable.end(), "Trying to create a node which already exists.");

        auto node = nodeFactory[type](nodeId, sampleRate, blockSize);
        nodeTable.insert({nodeId, node});
        edgeTable.insert({nodeId, {}});
    }

    template <typename FloatType>
    void Runtime<FloatType>::deleteNode(std::string const& hash)
    {
        ELEM_DBG("[Native] deleteNode " << hash);

        int64_t nodeId = std::stoll(hash, nullptr, 16);
        invariant(nodeTable.find(nodeId) != nodeTable.end(), "Trying to delete an unrecognized node.");
        invariant(edgeTable.find(nodeId) != edgeTable.end(), "Trying to delete an unrecognized node.");

        // Move the node out of the nodeTable. It will be pruned from the garbageTable
        // asynchronously after the renderer has dropped its references.
        garbageTable.insert(nodeTable.extract(nodeId));
        edgeTable.erase(nodeId);
    }

    template <typename FloatType>
    void Runtime<FloatType>::setProperty(std::string const& hash, std::string const& prop, js::Value const& v)
    {
        ELEM_DBG("[Native] setProperty " << hash << " " << prop << " " << v.toString());

        int64_t nodeId = std::stoll(hash, nullptr, 16);
        invariant(nodeTable.find(nodeId) != nodeTable.end(), "Trying to set a property for an unrecognized node.");

        // This is intentionally called on the non-realtime thread. It is the job
        // of the GraphNode to ensure thread safety between calls to setProperty
        // and calls to its own proces function
        nodeTable.at(nodeId)->setProperty(prop, v, sharedResourceMap);
    }

    template <typename FloatType>
    void Runtime<FloatType>::appendChild(std::string const& parentHash, std::string const& childHash)
    {
        ELEM_DBG("[Native] appendChild " << childHash << " to parent " << parentHash);

        int64_t parentId = std::stoll(parentHash, nullptr, 16);
        int64_t childId = std::stoll(childHash, nullptr, 16);

        invariant(nodeTable.find(parentId) != nodeTable.end(), "Trying to append a child to an unknown parent.");
        invariant(edgeTable.find(parentId) != edgeTable.end(), "Trying to append a child to an unknown parent.");
        invariant(nodeTable.find(childId) != nodeTable.end(), "Trying to append an unknown child to a parent.");

        edgeTable.at(parentId).push_back(childId);
    }

    template <typename FloatType>
    void Runtime<FloatType>::activateRoots(js::Array const& roots)
    {
        ELEM_DBG("[Native] activateRoots " << roots);

        // Populate and activate from the incoming event
        std::set<GraphNodeId> active;

        for (auto const& v : roots) {
            auto const nodeHash = (js::String) v;
            int64_t nodeId = std::stoll(nodeHash, nullptr, 16);

            invariant(nodeTable.find(nodeId) != nodeTable.end(), "Trying to activate an unrecognized root node.");

            if (auto ptr = std::dynamic_pointer_cast<RootNode<FloatType>>(nodeTable.at(nodeId))) {
                ptr->activate(); // TODO: Do this with props
                active.insert(nodeId);
            }
        }

        // Deactivate any prior roots
        for (auto const& n : currentRoots) {
            if (auto ptr = std::dynamic_pointer_cast<RootNode<FloatType>>(nodeTable.at(n))) {
                // If any current root was not marked active in this event, we deactivate it
                if (active.count(n) == 0) {
                    ptr->deactivate();
                }

                // And if it's still running, we hang onto it
                if (ptr->stillRunning()) {
                    active.insert(n);
                }
            }
        }

        // Merge
        currentRoots = active;
    }

    //==============================================================================
    template <typename FloatType>
    void Runtime<FloatType>::processQueuedEvents(std::function<void(std::string const&, js::Value)> evtCallback)
    {
        // TODO: Before the runtime refactor, we would use this opportunity to check to see if the graphRenderer
        // had raised any process flags to signal a realtime rendering issue. That was originally implemented
        // before the rendering procedure became iterative. Now that it's iterative, let's refactor that side
        // to return a boolean from its process call, then we can handle flag raising and error dispatching
        // here easily.

        // Visit all nodes to service any events they may want to relay
        for (auto it = nodeTable.begin(); it != nodeTable.end(); ++it) {
            it->second->processEvents(evtCallback);
        }
    }

    template <typename FloatType>
    void Runtime<FloatType>::reset()
    {
        // TODO: Right now only the SampleNode actually does anything with reset(). Need
        // to add this behavior to nodes like delay and convolve and then should also do
        // a pass here through the tapTable bufers. Edit: wait, don't need that, just need
        // the TapOut nodes to zero themselves out, w00t
        for (auto it = nodeTable.begin(); it != nodeTable.end(); ++it) {
            it->second->reset();
        }
    }

    //==============================================================================
    template <typename FloatType>
    void Runtime<FloatType>::updateSharedResourceMap(std::string const& name, FloatType const* data, size_t size)
    {
        sharedResourceMap.insert(
            name,
            std::make_shared<typename SharedResourceBuffer<FloatType>::element_type>(data, data + size)
        );
    }

    template <typename FloatType>
    void Runtime<FloatType>::registerNodeType(std::string const& type, Runtime::NodeFactoryFn && fn)
    {
        invariant(nodeFactory.find(type) == nodeFactory.end(), "Trying to install a node type which already exists");
        nodeFactory.emplace(type, std::move(fn));
    }

    //==============================================================================
    template <typename FloatType>
    void Runtime<FloatType>::traverse(std::set<GraphNodeId>& visited, std::vector<GraphNodeId>& visitOrder, GraphNodeId const& n) {
        // If we've already visited this node, skip
        if (visited.count(n) > 0)
            return;

        auto& children = edgeTable.at(n);
        auto const numChildren = children.size();

        for (size_t i = 0; i < numChildren; ++i) {
            traverse(visited, visitOrder, children.at(i));
        }

        visitOrder.push_back(n);
        visited.insert(n);
    }

    template <typename FloatType>
    std::shared_ptr<GraphRenderSequence<FloatType>> Runtime<FloatType>::buildRenderSequence()
    {
        // Grab a fresh render sequence
        auto rseq = renderSeqPool.allocate();

        // Clear in case it was already used
        rseq->reset();

        // Reset our buffer allocator
        bufferAllocator.reset();

        // Capture tap nodes for the pre-processing step
        for (auto const& [nid, node] : nodeTable) {
            if (auto ptr = std::dynamic_pointer_cast<TapOutNode<FloatType>>(node)) {
                rseq->pushTap(ptr);
            }
        }

        // Keep track of visits
        std::set<GraphNodeId> visited;

        // Here we iterate all current roots and visit the graph from each
        // root, pushing onto the render sequence.
        //
        // TODO currentRoots is a set of graph node ids. We need to copy the set into a vector
        // and then sort it to make sure we visit the active nodes first, and the deactivated nodes
        // afterwards. That way, once the deactivated nodes finish their fade out, we can just stop
        // processing their subtrees without risking skipping a node needed by another root.
        //
        // std::vector<GraphNodeId> sorted(currentRoots.begin(), currentRoots.end());
        // std::sort(sorted.begin(), sorted.end(), [=](GraphNodeId a, GraphNodeId b) {
        //     auto na = nodeTable.at(a);
        //     auto nb = nodeTable.at(b);
        //
        //     return a.isActive(); // something like that
        // });
        for (auto const& n : currentRoots) {
            if (auto ptr = std::dynamic_pointer_cast<RootNode<FloatType>>(nodeTable.at(n))) {
                // The GraphRenderSequence should make RootRenderSequence instances already provisioned
                // with the buffer map access. auto rrs = rseq->newRootSequence(ptr);
                RootRenderSequence<FloatType> rrs(rseq->bufferMap, ptr);

                std::vector<GraphNodeId> visitOrder;
                traverse(visited, visitOrder, ptr->getId());

                std::for_each(visitOrder.begin(), visitOrder.end(), [&](GraphNodeId const& nid) {
                    auto& node = nodeTable.at(nid);
                    auto& children = edgeTable.at(nid);

                    if (children.size() > 0) {
                        rrs.push(bufferAllocator, node, children);
                    } else {
                        rrs.push(bufferAllocator, node);
                    }
                });

                rseq->push(std::move(rrs));
            }
        }

        return rseq;
    }

} // namespace elem
