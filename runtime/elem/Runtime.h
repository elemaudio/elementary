#pragma once

#include <memory>
#include <set>
#include <unordered_map>

#include "builtins/helpers/RefCountedPool.h"
#include "DefaultNodeTypes.h"
#include "GraphNode.h"
#include "GraphRenderSequence.h"
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
        int applyInstructions(js::Array const& batch);

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

        // Releases unused graph nodes
        //
        // Returns the set of graph nodes that have been cleared, which should be communicated
        // to the respective Renderer instance on the JavaScript side to ensure matching node maps
        std::set<NodeId> gc();

        //==============================================================================
        // Loads a shared resource into memory, taking ownership of the given pointer.
        //
        // This method populates an internal map from which any GraphNode can request a
        // shared pointer to the resource.
        bool addSharedResource(std::string const& name, std::unique_ptr<SharedResource> resource);

        // Removes unused resources from the map
        //
        // This method will retain any resource with references held by an active
        // audio graph.
        void pruneSharedResources();

        // Returns an iterator through the names of the entries in the shared resoure map.
        //
        // Intentionally, this does not provide access to the values in the map.
        SharedResourceMap::KeyViewType getSharedResourceMapKeys();

        //==============================================================================
        // For registering custom GraphNode factory functions.
        //
        // New node types must inherit GraphNode, and the factory function must produce a shared
        // pointer to a new node instance.
        //
        // After registering your node type, the runtime instance will be ready to receive
        // instructions for your new type, such as those produced by the frontend
        // from, e.g., `core.render(createNode("myNewNodeType", props, [children]))`
        using NodeFactoryFn = std::function<std::shared_ptr<GraphNode<FloatType>>(NodeId const id, double sampleRate, int const blockSize)>;
        int registerNodeType (std::string const& type, NodeFactoryFn && fn);

        //==============================================================================
        // Returns a copy of the internal graph state representing all known nodes and properties
        js::Object snapshot();

    private:
        //==============================================================================
        // The rendering interface
        enum class InstructionType {
          CREATE_NODE = 0,
          APPEND_CHILD = 2,
          SET_PROPERTY = 3,
          ACTIVATE_ROOTS = 4,
          COMMIT_UPDATES = 5,
        };

        int createNode(js::Value const& nodeId, js::Value const& type);
        int setProperty(js::Value const& nodeId, js::Value const& prop, js::Value const& v);
        int appendChild(js::Value const& parentId, js::Value const& childId, js::Value const& childOutputChannel);
        int activateRoots(js::Array const& v);

        BufferAllocator<FloatType> bufferAllocator;
        std::shared_ptr<GraphRenderSequence<FloatType>> rtRenderSeq;
        std::shared_ptr<GraphRenderSequence<FloatType>> buildRenderSequence();
        void traverse(std::set<NodeId>& visited, std::vector<NodeId>& visitOrder, NodeId const& n);

        SingleWriterSingleReaderQueue<std::shared_ptr<GraphRenderSequence<FloatType>>> rseqQueue;

        //==============================================================================
        std::unordered_map<std::string, NodeFactoryFn> nodeFactory;
        std::unordered_map<NodeId, std::shared_ptr<GraphNode<FloatType>>> nodeTable;
        std::unordered_map<NodeId, std::vector<std::pair<NodeId, size_t>>> edgeTable;

        std::set<NodeId> currentRoots;
        RefCountedPool<GraphRenderSequence<FloatType>> renderSeqPool;

        SharedResourceMap sharedResourceMap;

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
    int Runtime<FloatType>::applyInstructions(elem::js::Array const& batch)
    {
        bool shouldRebuild = false;

        // TODO: For correct transaction semantics here, we should createNode into a separate
        // map that only gets merged into the actual nodeMap on commitUpdaes
        for (auto& next : batch) {
            if (!next.isArray())
                return ReturnCode::InvalidInstructionFormat();

            auto const& ar = next.getArray();

            if(!ar[0].isNumber())
                return ReturnCode::InvalidInstructionFormat();

            auto const cmd = static_cast<InstructionType>(static_cast<int>((elem::js::Number) ar[0]));
            auto res = ReturnCode::Ok();

            switch (cmd) {
                case InstructionType::CREATE_NODE:
                    res = createNode(ar[1], ar[2]);
                    break;
                case InstructionType::SET_PROPERTY:
                    res = setProperty(ar[1], ar[2], ar[3]);
                    break;
                case InstructionType::APPEND_CHILD:
                    res = appendChild(ar[1], ar[2], ar[3]);
                    break;
                case InstructionType::ACTIVATE_ROOTS:
                    res = activateRoots(ar[1]);
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

            // TODO: And here we should abort the transaction and revert any applied properties
            if (res != ReturnCode::Ok()) {
                return res;
            }
        }

        return ReturnCode::Ok();
    }

    template <typename FloatType>
    std::set<NodeId> Runtime<FloatType>::gc()
    {
        // First, reset all RenderSeq instances in the pool that are not
        // currently in use
        renderSeqPool.forEach([](auto&& rseq) {
            if (rseq.use_count() == 1) {
                rseq->reset();
            }
        });

        std::set<NodeId> pruned;

        // Now we can be sure that the nodes that are currently involved in the
        // active graph rendering sequence have multiple references, while those
        // that are unused are held only by the nodeTable. Those nodes we clean up here.
        for (auto it = nodeTable.begin(); it != nodeTable.end(); /* skip increment */) {
            if (it->second.use_count() == 1) {
                ELEM_DBG("[Native] gc " << nodeIdToHex(it->second->getId()));
                pruned.insert(it->first);
                edgeTable.erase(it->first);
                it = nodeTable.erase(it);
            } else {
                it++;
            }
        }

        return pruned;
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
    int Runtime<FloatType>::createNode(js::Value const& a1, js::Value const& a2)
    {
        if (!a1.isNumber() || !a2.isString())
            return ReturnCode::InvalidInstructionFormat();

        auto const nodeId = static_cast<int32_t>((js::Number) a1);
        auto const type = (js::String) a2;

        ELEM_DBG("[Native] createNode " << type << "#" << nodeIdToHex(nodeId));

        if (nodeFactory.find(type) == nodeFactory.end())
            return ReturnCode::UnknownNodeType();
        if (nodeTable.find(nodeId) != nodeTable.end() || edgeTable.find(nodeId) != edgeTable.end())
            return ReturnCode::NodeAlreadyExists();

        auto node = nodeFactory[type](nodeId, sampleRate, blockSize);
        nodeTable.insert({nodeId, node});
        edgeTable.insert({nodeId, {}});

        return ReturnCode::Ok();
    }

    template <typename FloatType>
    int Runtime<FloatType>::setProperty(js::Value const& a1, js::Value const& a2, js::Value const& v)
    {
        if (!a1.isNumber() || !a2.isString())
            return ReturnCode::InvalidInstructionFormat();

        auto const nodeId = static_cast<int32_t>((js::Number) a1);
        auto const prop = (js::String) a2;

        ELEM_DBG("[Native] setProperty " << nodeIdToHex(nodeId) << " " << prop << " " << v.toString());

        if (nodeTable.find(nodeId) == nodeTable.end())
            return ReturnCode::NodeNotFound();

        // This is intentionally called on the non-realtime thread. It is the job
        // of the GraphNode to ensure thread safety between calls to setProperty
        // and calls to its own proces function
        return nodeTable.at(nodeId)->setProperty(prop, v, sharedResourceMap);
    }

    template <typename FloatType>
    int Runtime<FloatType>::appendChild(js::Value const& a1, js::Value const& a2, js::Value const& a3)
    {
        if (!a1.isNumber() || !a2.isNumber() || !a3.isNumber())
            return ReturnCode::InvalidInstructionFormat();

        auto const parentId = static_cast<int32_t>((js::Number) a1);
        auto const childId = static_cast<int32_t>((js::Number) a2);
        auto const childOutputChannel = static_cast<int32_t>((js::Number) a3);

        ELEM_DBG("[Native] appendChild " << nodeIdToHex(childId) << ":" << childOutputChannel << " to parent " << nodeIdToHex(parentId));

        if (nodeTable.find(parentId) == nodeTable.end() || edgeTable.find(parentId) == edgeTable.end())
            return ReturnCode::NodeNotFound();
        if (nodeTable.find(childId) == nodeTable.end())
            return ReturnCode::NodeNotFound();

        edgeTable.at(parentId).push_back({childId, childOutputChannel});

        return ReturnCode::Ok();
    }

    template <typename FloatType>
    int Runtime<FloatType>::activateRoots(js::Array const& roots)
    {
        // Populate and activate from the incoming event
        std::set<NodeId> active;

        for (auto const& v : roots)
        {
            if (!v.isNumber())
            {
                ELEM_DBG("[Error] activateRoot - Invalid nodeId format.");
                return ReturnCode::InvalidInstructionFormat();
            }

            int32_t nodeId = static_cast<int32_t>((js::Number)v);
            ELEM_DBG("[Native] activateRoot " << nodeIdToHex(nodeId));

            // Using find() method for safe access to nodeTable elements
            auto it = nodeTable.find(nodeId);
            if (it == nodeTable.end())
            {
                ELEM_DBG("[Error] activateRoot - NodeId not found: " << nodeIdToHex(nodeId));
                return ReturnCode::NodeNotFound();
            }

            // Attempt to cast the found node to a RootNode type and activate it
            auto ptr = std::dynamic_pointer_cast<RootNode<FloatType>>(it->second);
            if (ptr)
            {
                ptr->activate(currentRoots.empty() ? FloatType(1) : FloatType(0));
                active.insert(nodeId);
                ELEM_DBG("[Success] Activated root: " << nodeIdToHex(nodeId));
            }
            else
            {
                ELEM_DBG("[Error] Failed to cast to RootNode or activate: " << nodeIdToHex(nodeId));
            }
        }

        // Deactivate any prior roots not included in the incoming active set
        for (auto const& n : currentRoots)
        {
            auto it = nodeTable.find(n);
            if (it != nodeTable.end())
            {
                auto ptr = std::dynamic_pointer_cast<RootNode<FloatType>>(it->second);
                // If any current root was not marked active in this event, we deactivate it
                if (ptr)
                {
                    if (active.count(n) == 0)
                    {
                        ptr->setProperty("active", false);
                        ELEM_DBG("[Success] Deactivated root: " << nodeIdToHex(n));
                    }
                    // And if it's still running, we hang onto it
                    if (ptr->stillRunning())
                    {
                        active.insert(n);
                    }
                }
            }
        }
        // Merge
        currentRoots.swap(active);
        return ReturnCode::Ok();
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
    bool Runtime<FloatType>::addSharedResource(std::string const& name, std::unique_ptr<SharedResource> resource)
    {
        return sharedResourceMap.add(name, std::move(resource));
    }

    template <typename FloatType>
    void Runtime<FloatType>::pruneSharedResources()
    {
        sharedResourceMap.prune();
    }

    template <typename FloatType>
    SharedResourceMap::KeyViewType Runtime<FloatType>::getSharedResourceMapKeys()
    {
        return sharedResourceMap.keys();
    }

    template <typename FloatType>
    int Runtime<FloatType>::registerNodeType(std::string const& type, Runtime::NodeFactoryFn && fn)
    {
        if (nodeFactory.find(type) != nodeFactory.end())
            return ReturnCode::NodeTypeAlreadyExists();

        nodeFactory.emplace(type, std::move(fn));
        return ReturnCode::Ok();
    }

    template <typename FloatType>
    js::Object Runtime<FloatType>::snapshot()
    {
        js::Object ret;

        for (auto& [nodeId, node] : nodeTable) {
            ret.insert({nodeIdToHex(nodeId), node->getProperties()});
        }

        return ret;
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
            auto const& [childId, outputChannel] = children.at(i);
            traverse(visited, visitOrder, childId);
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
