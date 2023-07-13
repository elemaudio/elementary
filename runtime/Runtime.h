#pragma once

#include <memory>
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
            size_t numInputChannels,
            FloatType** outputChannelData,
            size_t numOutputChannels,
            size_t numSamples,
            void* userData = nullptr);

        //==============================================================================
        // Process queued events
        //
        // This raises events from the processing graph such as new data from analysis nodes
        // or errors encountered while processing.
        void processQueuedEvents(std::function<void(std::string const&, js::Value)>&& evtCallback);

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
        using NodeFactoryFn = std::function<std::shared_ptr<GraphNode<FloatType>>(NodeId const id, double sampleRate, int const blockSize)>;
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

        void createNode(int32_t const& nodeId, std::string const& type);
        void deleteNode(int32_t const& nodeId);
        void setProperty(int32_t const& nodeId, std::string const& prop, js::Value const& v);
        void appendChild(int32_t const& parentId, int32_t const& childId);
        void activateRoots(js::Array const& v);

        BufferAllocator<FloatType> bufferAllocator;
        std::shared_ptr<GraphRenderSequence<FloatType>> rtRenderSeq;
        std::shared_ptr<GraphRenderSequence<FloatType>> buildRenderSequence();
        void traverse(std::set<NodeId>& visited, std::vector<NodeId>& visitOrder, NodeId const& n);

        SingleWriterSingleReaderQueue<std::shared_ptr<GraphRenderSequence<FloatType>>> rseqQueue;

        //==============================================================================
        std::unordered_map<std::string, NodeFactoryFn> nodeFactory;
        std::unordered_map<NodeId, std::shared_ptr<GraphNode<FloatType>>> nodeTable;
        std::unordered_map<NodeId, std::vector<NodeId>> edgeTable;
        std::unordered_map<NodeId, std::shared_ptr<GraphNode<FloatType>>> garbageTable;

        std::set<NodeId> currentRoots;
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
        bool shouldRebuild = false;

        // TODO: For correct transaction semantics here, we should createNode into a separate
        // map that only gets merged into the actual nodeMap on commitUpdaes
        for (auto& next : batch) {
            invariant(next.isArray(), "Expected a command array.");
            auto const& ar = next.getArray();

            invariant(ar[0].isNumber(), "Expected a number type command.");
            auto const cmd = static_cast<InstructionType>(static_cast<int>((elem::js::Number) ar[0]));

            auto varToInt = [](elem::js::Value const& v) -> int32_t {
                invariant(v.isNumber(), "Expected a number type node identifier. Make sure you are using @elemaudio/core@v2.0+");
                return static_cast<int32_t>((elem::js::Number) v);
            };

            switch (cmd) {
                case InstructionType::CREATE_NODE:
                    createNode(varToInt(ar[1]), (elem::js::String) ar[2]);
                    break;
                case InstructionType::DELETE_NODE:
                    deleteNode(varToInt(ar[1]));
                    break;
                case InstructionType::SET_PROPERTY:
                    setProperty(varToInt(ar[1]), (elem::js::String) ar[2], ar[3]);
                    break;
                case InstructionType::APPEND_CHILD:
                    appendChild(varToInt(ar[1]), varToInt(ar[2]));
                    break;
                case InstructionType::ACTIVATE_ROOTS:
                    activateRoots(ar[1].getArray());
                    shouldRebuild = true;
                    break;
                case InstructionType::COMMIT_UPDATES:
                    if (shouldRebuild) {
                        rseqQueue.push(buildRenderSequence());
                    }
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
                ELEM_DBG("[Native] pruneNode " << nodeIdToHex(it->second->getId()));
                it = garbageTable.erase(it);
            } else {
                it++;
            }
        }
    }

    template <typename FloatType>
    void Runtime<FloatType>::process(const FloatType** inputChannelData, size_t numInputChannels, FloatType** outputChannelData, size_t numOutputChannels, size_t numSamples, void* userData)
    {
        if (rseqQueue.size() > 0) {
            std::shared_ptr<GraphRenderSequence<FloatType>> rseq;

            while (rseqQueue.size() > 0) {
                rseqQueue.pop(rseq);
            }

            rtRenderSeq = rseq;
        }

        if (rtRenderSeq) {
            rtRenderSeq->process(inputChannelData, numInputChannels, outputChannelData, numOutputChannels, numSamples, userData);
        }
    }

    //==============================================================================
    template <typename FloatType>
    void Runtime<FloatType>::createNode(int32_t const& nodeId, std::string const& type)
    {
        ELEM_DBG("[Native] createNode " << type << "#" << nodeIdToHex(nodeId));

        invariant(nodeFactory.find(type) != nodeFactory.end(), "Unknown node type " + type);
        invariant(nodeTable.find(nodeId) == nodeTable.end(), "Trying to create a node which already exists.");
        invariant(edgeTable.find(nodeId) == edgeTable.end(), "Trying to create a node which already exists.");

        auto node = nodeFactory[type](nodeId, sampleRate, blockSize);
        nodeTable.insert({nodeId, node});
        edgeTable.insert({nodeId, {}});
    }

    template <typename FloatType>
    void Runtime<FloatType>::deleteNode(int32_t const& nodeId)
    {
        ELEM_DBG("[Native] deleteNode " << nodeIdToHex(nodeId));

        invariant(nodeTable.find(nodeId) != nodeTable.end(), "Trying to delete an unrecognized node.");
        invariant(edgeTable.find(nodeId) != edgeTable.end(), "Trying to delete an unrecognized node.");

        // Move the node out of the nodeTable. It will be pruned from the garbageTable
        // asynchronously after the renderer has dropped its references.
        garbageTable.insert(nodeTable.extract(nodeId));
        edgeTable.erase(nodeId);
    }

    template <typename FloatType>
    void Runtime<FloatType>::setProperty(int32_t const& nodeId, std::string const& prop, js::Value const& v)
    {
        ELEM_DBG("[Native] setProperty " << nodeIdToHex(nodeId) << " " << prop << " " << v.toString());

        invariant(nodeTable.find(nodeId) != nodeTable.end(), "Trying to set a property for an unrecognized node.");

        // This is intentionally called on the non-realtime thread. It is the job
        // of the GraphNode to ensure thread safety between calls to setProperty
        // and calls to its own proces function
        nodeTable.at(nodeId)->setProperty(prop, v, sharedResourceMap);
    }

    template <typename FloatType>
    void Runtime<FloatType>::appendChild(int32_t const& parentId, int32_t const& childId)
    {
        ELEM_DBG("[Native] appendChild " << nodeIdToHex(childId) << " to parent " << nodeIdToHex(parentId));

        invariant(nodeTable.find(parentId) != nodeTable.end(), "Trying to append a child to an unknown parent.");
        invariant(edgeTable.find(parentId) != edgeTable.end(), "Trying to append a child to an unknown parent.");
        invariant(nodeTable.find(childId) != nodeTable.end(), "Trying to append an unknown child to a parent.");

        edgeTable.at(parentId).push_back(childId);
    }

    template <typename FloatType>
    void Runtime<FloatType>::activateRoots(js::Array const& roots)
    {
        // Populate and activate from the incoming event
        std::set<NodeId> active;

        for (auto const& v : roots) {
            int32_t nodeId = static_cast<int32_t>((js::Number) v);
            ELEM_DBG("[Native] activateRoot " << nodeIdToHex(nodeId));

            invariant(nodeTable.find(nodeId) != nodeTable.end(), "Trying to activate an unrecognized root node.");

            if (auto ptr = std::dynamic_pointer_cast<RootNode<FloatType>>(nodeTable.at(nodeId))) {
                ptr->setProperty("active", true);
                active.insert(nodeId);
            }
        }

        // Deactivate any prior roots
        for (auto const& n : currentRoots) {
            if (auto ptr = std::dynamic_pointer_cast<RootNode<FloatType>>(nodeTable.at(n))) {
                // If any current root was not marked active in this event, we deactivate it
                if (active.count(n) == 0) {
                    ptr->setProperty("active", false);
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
    void Runtime<FloatType>::processQueuedEvents(std::function<void(std::string const&, js::Value)>&& evtCallback)
    {
        // This looks a little shady, but because of the atomic ref count in std::shared_ptr this assignment
        // is indeed thread-safe
        if (auto ptr = rtRenderSeq)
        {
            ptr->processQueuedEvents(std::move(evtCallback));
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
    void Runtime<FloatType>::traverse(std::set<NodeId>& visited, std::vector<NodeId>& visitOrder, NodeId const& n) {
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

        // Here we iterate all current roots and visit the graph from each
        // root, pushing onto the render sequence.
        //
        // We're careful to traverse the graph starting from all active root
        // nodes before we traverse from any roots marked inactive. This way, after an
        // inactive root node has finished its fade, we can safely stop running its
        // associated RootRenderSequence knowing that any nodes in the dependency graph
        // of the active roots will still get visited.
        //
        // To put that another way, we make sure that all "live" nodes are associated with
        // the RootRenderSequences of the active roots, and any nodes which will be "dead" after
        // the associated root has finished its fade can be safely skipped.
        std::list<std::shared_ptr<RootNode<FloatType>>> sortedRoots;

        // Keep track of visits
        std::set<NodeId> visited;

        for (auto const& n : currentRoots) {
            if (auto ptr = std::dynamic_pointer_cast<RootNode<FloatType>>(nodeTable.at(n))) {
                auto isActive = ptr->getPropertyWithDefault("active", (js::Boolean) false);

                if (isActive) {
                    sortedRoots.push_front(ptr);
                } else {
                    sortedRoots.push_back(ptr);
                }
            }
        }

        for (auto& ptr : sortedRoots) {
            RootRenderSequence<FloatType> rrs(rseq->bufferMap, ptr);

            std::vector<NodeId> visitOrder;
            traverse(visited, visitOrder, ptr->getId());

            std::for_each(visitOrder.begin(), visitOrder.end(), [&](NodeId const& nid) {
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

        return rseq;
    }

} // namespace elem
