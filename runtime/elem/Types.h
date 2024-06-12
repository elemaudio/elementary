#pragma once

#include <sstream>
#include <unordered_map>


namespace elem
{

    //==============================================================================
    // Our graph node identifier type
    using NodeId = int32_t;

    // A simple helper for pretty printing NodeId types
    inline std::string nodeIdToHex (NodeId i) {
        std::stringstream ss;
        ss << std::hex << i;

        auto s = ss.str();

        if (s.size() <= 8) {
            s.insert(0, 8 - s.size(), '0');
        }

        return "0x" + s;
    }

    //==============================================================================
    // A static helper for enumerating and describing return codes used throughout
    // the codebase
    struct ReturnCode {
        static int Ok()                         { return 0; }
        static int UnknownNodeType()            { return 1; }
        static int NodeNotFound()               { return 2; }
        static int NodeAlreadyExists()          { return 3; }
        static int NodeTypeAlreadyExists()      { return 4; }
        static int InvalidPropertyType()        { return 5; }
        static int InvalidPropertyValue()       { return 6; }
        static int InvariantViolation()         { return 7; }
        static int InvalidInstructionFormat()   { return 8; }

        static std::string describe (int c) {
            switch (c) {
                case 0:
                    return "Ok";
                case 1:
                    return "Node type not recognized";
                case 2:
                    return "Node not found";
                case 3:
                    return "Attempting to create a node that already exists";
                case 4:
                    return "Attempting to create a node type that already exists";
                case 5:
                    return "Invalid value type for the given node property";
                case 6:
                    return "Invalid value for the given node property";
                case 7:
                    return "Invariant violation";
                case 8:
                    return "Invalid instruction format";
                default:
                    return "Return code not recognized";
            }
        }
    };

    //==============================================================================
    // A simple struct representing the inputs to a given GraphNode during the realtime
    // audio block processing step.
    template <typename FloatType>
    struct BlockContext
    {
        FloatType const** inputData;
        size_t numInputChannels;
        FloatType** outputData;
        size_t numOutputChannels;
        size_t numSamples;
        void* userData;
        bool active;
    };

    //==============================================================================
    // A std::span-like view over a block of audio data
    template <typename FloatType>
    class BufferView {
    public:
        BufferView(FloatType* __data, size_t __size)
            : _data(__data), _size(__size)
        {}

        FloatType operator[] (size_t i) {
            return _data[i];
        }

        FloatType* data() const { return _data; }
        size_t size() const { return _size; }

    private:
        FloatType* _data;
        size_t _size;
    };

    //==============================================================================
    // A couple of quick helpers to provide an API similar to std::views::keys of C++20.
    //
    // This allows the Runtime to expose the keys of the shared resource map to the end
    // user without exposing the values.
    template<class MapType>
    class KeyIterator : public MapType::iterator
    {
    public:
        typedef typename MapType::iterator map_iterator;
        typedef typename map_iterator::value_type::first_type key_type;

        KeyIterator(const map_iterator& other) : MapType::iterator(other) {} ;

        key_type& operator *()
        {
            return MapType::iterator::operator*().first;
        }
    };

    template<class MapType>
    struct MapKeyView {
    public:
        MapKeyView(MapType& m)
            : other(m) {}

        auto begin()    { return KeyIterator<MapType>(other.begin()); }
        auto end()      { return KeyIterator<MapType>(other.end()); }

    private:
        MapType& other;
    };

} // namespace elem
