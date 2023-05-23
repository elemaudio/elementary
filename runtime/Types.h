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
    template <typename FloatType>
    using SharedResourceBuffer = std::shared_ptr<std::vector<FloatType> const>;

    template <typename FloatType>
    using MutableSharedResourceBuffer = std::shared_ptr<std::vector<FloatType>>;

    template <typename FloatType>
    class SharedResourceMap {
    public:
        //==============================================================================
        SharedResourceMap() = default;

        //==============================================================================
        // Accessor methods for immutable resources
        void insert(std::string const& p, SharedResourceBuffer<FloatType>&& srb);
        bool has(std::string const& p);
        SharedResourceBuffer<FloatType> const& get(std::string const& p);

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
    void SharedResourceMap<FloatType>::insert (std::string const& p, SharedResourceBuffer<FloatType>&& srb) {
        // TODO: This is risky. In the case that a key is already present and we assign to it, it could be
        // that there's a realtime node somewhere who is also holding a shared_ptr to the existing resource.
        // If the resource map then drops its reference, it may leave the realtime node holding the last one,
        // in which case a dealloc there could cause memory troubles on the realtime thread.
        //
        // Is there any reason not to enforce that this map is itself immutable? I.e. you can only ever add to it, you can't
        // change existing entries? That would give the guarantees we need here.
        imms.insert_or_assign(p, std::move(srb));
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
    MutableSharedResourceBuffer<FloatType> const& SharedResourceMap<FloatType>::getOrCreateMutable (std::string const& p, size_t blockSize) {
        if (muts.count(p) > 0) {
            return muts.at(p);
        }

        muts.emplace(p, std::make_shared<std::vector<FloatType>>(blockSize));
        std::fill_n(muts.at(p)->data(), blockSize, FloatType(0));
        return muts.at(p);
    }

} // namespace elem
