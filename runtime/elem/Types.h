#pragma once

#include <memory>
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
        FloatType* outputData;
        size_t numSamples;
        void* userData;
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

    //==============================================================================
    template <typename FloatType>
    using SharedResourceBuffer = std::shared_ptr<std::vector<FloatType> const>;

    template <typename FloatType>
    using MutableSharedResourceBuffer = std::shared_ptr<std::vector<FloatType>>;

    template <typename FloatType>
    class SharedResourceMap {
    public:
        //==============================================================================
        using KeyViewType = MapKeyView<std::unordered_map<std::string, SharedResourceBuffer<FloatType>>>;

        //==============================================================================
        SharedResourceMap() = default;

        //==============================================================================
        // Accessor methods for immutable resources
        bool insert(std::string const& p, SharedResourceBuffer<FloatType>&& srb);
        bool has(std::string const& p);
        SharedResourceBuffer<FloatType> const& get(std::string const& p);

        void prune();
        KeyViewType keys();

        //==============================================================================
        // Accessor methods for mutable resources
        MutableSharedResourceBuffer<FloatType> const& getOrCreateMutable(std::string const& p, size_t blockSize);

    private:
        std::unordered_map<std::string, SharedResourceBuffer<FloatType>> imms;
        std::unordered_map<std::string, MutableSharedResourceBuffer<FloatType>> muts;
    };

    //==============================================================================
    // Details...
    template <typename FloatType>
    bool SharedResourceMap<FloatType>::insert (std::string const& p, SharedResourceBuffer<FloatType>&& srb) {
        // We only allow insertions here, not updates. This preserves immutability of existing
        // entries which we need in case any active graph nodes hold references to those entries.
        return imms.emplace(p, std::move(srb)).second;
    }

    template <typename FloatType>
    bool SharedResourceMap<FloatType>::has (std::string const& p) {
        return imms.count(p) > 0;
    }

    template <typename FloatType>
    SharedResourceBuffer<FloatType> const& SharedResourceMap<FloatType>::get (std::string const& p) {
        return imms.at(p);
    }

    template <typename FloatType>
    void SharedResourceMap<FloatType>::prune() {
        for (auto it = imms.cbegin(); it != imms.cend(); /* no increment */) {
            if (it->second.use_count() == 1) {
                imms.erase(it++);
            } else {
                it++;
            }
        }
    }

    template <typename FloatType>
    typename SharedResourceMap<FloatType>::KeyViewType SharedResourceMap<FloatType>::keys() {
        return MapKeyView<decltype(imms)>(imms);
    }

    template <typename FloatType>
    MutableSharedResourceBuffer<FloatType> const& SharedResourceMap<FloatType>::getOrCreateMutable (std::string const& p, size_t blockSize) {
        if (muts.count(p) > 0) {
            return muts.at(p);
        }

        muts.emplace(p, std::make_shared<std::vector<FloatType>>(blockSize));
        std::fill_n(muts.at(p)->data(), blockSize, FloatType(0));
        return muts.at(p);
    }

} // namespace elem
