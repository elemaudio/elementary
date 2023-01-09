#pragma once

#include "Types.h"
#include "Value.h"

#include <unordered_map>


namespace elem
{

    //==============================================================================
    // Identifier type for GraphNode instances
    using GraphNodeId = int64_t;

    // A simple helper for returning a 0-padded hex string representing a
    // given node id.
    inline std::string graphNodeIdToString (GraphNodeId const& i) {
        std::stringstream ss;
        ss << std::hex << i;

        auto s = ss.str();
        s.insert(0, 8 - s.size(), '0');

        return s;
    }

    //==============================================================================
    // The GraphNode represents a single audio processing operation within the
    // larger audio graph.
    //
    // Users may implement custom operations by extending this class and registering
    // the new class with the GraphHost.
    template <typename FloatType>
    struct GraphNode {
        //==============================================================================
        using ValueType = FloatType;

        //==============================================================================
        // GraphNode constructor.
        //
        // The default implementation will hang onto the incoming NodeId and sample rate
        // in member variables. Users may extend this constructor to take the incoming
        // blockSize into account as well.
        GraphNode(GraphNodeId id, FloatType const sr, int const blockSize);

        //==============================================================================
        // GraphNode destructor.
        virtual ~GraphNode() = default;

        //==============================================================================
        // Returns the GraphNodeId associated with this node
        GraphNodeId getId() { return nodeId; }

        //==============================================================================
        FloatType getSampleRate() { return sampleRate; }
        size_t getBlockSize() { return blockSize; }

        //==============================================================================
        // Sets a property onto the graph node.
        //
        // By default this does nothing other than store the property into `props`,
        // but users may override this method to make changes to the way the node processes
        // audio according to incoming property values.
        //
        // Thread safety must be managed by the user. This method will be called on
        // a non-realtime thread.
        virtual void setProperty(std::string const& key, js::Value const& val);
        virtual void setProperty(std::string const& key, js::Value const& val, SharedResourceMap<FloatType>& resources);

        // Process the next block of audio data.
        //
        // Users must override this method when creating a custom GraphNode.
        //
        // This method will be called from the realtime thread. Thread safety
        // for any custom operations must be managed by the user.
        virtual void process (const FloatType** inputData,
                              FloatType* outputData,
                              size_t const numChannels,
                              size_t const numSamples,
                              int64_t sampleTime)  = 0;

        // Derived classes may override this method to relay events.
        //
        // This method will be called during GraphHost's `processQueuedEvents` on
        // the non-realtime thread. Any event to be relayed should be sent through the
        // event handler callback provided.
        //
        // Thread safety must be managed by the user.
        virtual void processEvents(std::function<void(std::string const&, js::Value)>& eventHandler) {}

        // Derived classes may override this method to reset themselves.
        //
        // This method will be called by the end user through Runtime/GraphHost
        // on the non-realtime thread. When receiving the call.
        virtual void reset() {}

        //==============================================================================
        GraphNodeId nodeId;
        FloatType sampleRate;
        std::unordered_map<std::string, js::Value> props;

    private:
        size_t blockSize;
    };

    //==============================================================================
    // Details...
    template <typename FloatType>
    GraphNode<FloatType>::GraphNode(GraphNodeId id, FloatType const sr, int const bs)
        : nodeId(id)
        , sampleRate(sr)
        , blockSize(static_cast<size_t>(bs)) {}

    template <typename FloatType>
    void GraphNode<FloatType>::setProperty(std::string const& key, js::Value const& val) {
        props.insert_or_assign(key, val);
    }

    template <typename FloatType>
    void GraphNode<FloatType>::setProperty(std::string const& key, js::Value const& val, SharedResourceMap<FloatType>&) {
        setProperty(key, val);
    }

} // namespace elem
