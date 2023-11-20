#pragma once

#include "Types.h"
#include "Value.h"

#include <unordered_map>
#include <functional>

namespace elem
{

    //==============================================================================
    // The GraphNode represents a single audio processing operation within the
    // larger audio graph.
    //
    // Users may implement custom operations by extending this class and registering
    // the new class with the GraphHost.
    template <typename FloatType>
    struct GraphNode {
        //==============================================================================
        // GraphNode constructor.
        //
        // The default implementation will hang onto the incoming NodeId and sample rate
        // in member variables. Users may extend this constructor to take the incoming
        // blockSize into account as well.
        GraphNode(NodeId id, double sr, size_t blockSize);

        //==============================================================================
        // GraphNode destructor.
        virtual ~GraphNode() = default;

        //==============================================================================
        // Returns the NodeId associated with this node
        NodeId getId() { return nodeId; }

        //==============================================================================
        double getSampleRate() { return sampleRate; }
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
        virtual int setProperty(std::string const& key, js::Value const& val);
        virtual int setProperty(std::string const& key, js::Value const& val, SharedResourceMap<FloatType>& resources);

        // Retreives a property from the Node's props, falling back to the provided
        // default value if no property exists by the given name.
        //
        // Properties are held in a map<std::string, js::Value>, but the ValueType template
        // allows fetching a property by a given subtype such as js::Number. If an entry is
        // found in the props map, its value will be returned by casting to ValueType, therefore
        // if the cast fails, this method will throw.
        template <typename ValueType>
        ValueType getPropertyWithDefault(std::string const& key, ValueType const& df);

        // Returns a copy of the entire property object in its current state
        js::Object getProperties();

        // Process the next block of audio data.
        //
        // Users must override this method when creating a custom GraphNode.
        //
        // This method will be called from the realtime thread. Thread safety
        // for any custom operations must be managed by the user.
        virtual void process (BlockContext<FloatType> const&)  = 0;

        // Derived classes may override this method to relay events.
        //
        // This method will be called during GraphHost's `processQueuedEvents` on
        // the non-realtime thread. Any event to be relayed should be sent through the
        // event handler callback provided.
        //
        // Thread safety must be managed by the user.
        virtual void processEvents(std::function<void(std::string const&, js::Value)>& /* eventHandler */) {}

        // Derived classes may override this method to reset themselves.
        //
        // This method will be called by the end user through Runtime/GraphHost
        // on the non-realtime thread. When receiving the call.
        virtual void reset() {}

    private:
        //==============================================================================
        NodeId nodeId;
        std::unordered_map<std::string, js::Value> props;

        double sampleRate;
        size_t blockSize;
    };

    //==============================================================================
    // Details...
    template <typename FloatType>
    GraphNode<FloatType>::GraphNode(NodeId id, double sr, size_t bs)
        : nodeId(id)
        , sampleRate(sr)
        , blockSize(bs)
    {
    }

    template <typename FloatType>
    int GraphNode<FloatType>::setProperty(std::string const& key, js::Value const& val) {
        props.insert_or_assign(key, val);
        return ReturnCode::Ok();
    }

    template <typename FloatType>
    int GraphNode<FloatType>::setProperty(std::string const& key, js::Value const& val, SharedResourceMap<FloatType>&) {
        return setProperty(key, val);
    }

    template <typename FloatType>
    template <typename ValueType>
    ValueType GraphNode<FloatType>::getPropertyWithDefault(std::string const& key, ValueType const& df) {
        if (props.count(key) > 0) {
            return static_cast<ValueType>(props.at(key));
        }

        return df;
    }

    template <typename FloatType>
    js::Object GraphNode<FloatType>::getProperties() {
        return js::Object(props.begin(), props.end());
    }

} // namespace elem
