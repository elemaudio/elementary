#pragma once

#include <memory>
#include <unordered_map>

#include "Types.h"


namespace elem
{

    //==============================================================================
    // Base class for shared resources which are made available to GraphNode
    // instances during property setting.
    class SharedResource {
    public:
      SharedResource() = default;
      virtual ~SharedResource() = default;

      virtual BufferView<float> getChannelData (size_t channelIndex) = 0;

      virtual size_t numChannels() = 0;
      virtual size_t numSamples() = 0;
    };

    //==============================================================================
    // Utility type definition
    using SharedResourcePtr = std::shared_ptr<SharedResource>;

    //==============================================================================
    // A small wrapper around an unordered_map for holding and interacting with
    // shared resources
    class SharedResourceMap {
    private:
        std::unordered_map<std::string, SharedResourcePtr> resources;

    public:
        SharedResourceMap() = default;

        using KeyViewType = MapKeyView<decltype(resources)>;

        // Accessor methods for resources
        //
        // We only allow insertions, not updates. This preserves immutability of existing
        // entries which we need in case any active graph nodes hold references to those entries.
        bool add(std::string const& name, SharedResourcePtr resource);
        bool has(std::string const& name) const;
        SharedResourcePtr get(std::string const& name) const;

        // Internal accessor for feedback tap nodes that supports constructing and
        // inserting a default entry
        SharedResourcePtr getTapResource(std::string const& name, std::function<SharedResourcePtr()> makeDefault);

        // Inspecting and clearing entries
        KeyViewType keys();
        void prune();
    };

    //==============================================================================
    // Details...
    inline bool SharedResourceMap::add (std::string const& name, SharedResourcePtr resource) {
        return resources.emplace(name, resource).second;
    }

    inline bool SharedResourceMap::has (std::string const& name) const {
        return resources.count(name) > 0;
    }

    inline SharedResourcePtr SharedResourceMap::get (std::string const& name) const {
        auto it = resources.find(name);

        if (it == resources.end())
            return nullptr;

        return it->second;
    }

    inline SharedResourcePtr SharedResourceMap::getTapResource(std::string const& name, std::function<SharedResourcePtr()> makeDefault) {
        auto it = resources.find(name);

        if (it != resources.end())
            return it->second;

        SharedResourcePtr resource = makeDefault();
        auto success = resources.emplace(name, resource).second;

        if (!success)
            return nullptr;

        return resource;
    }

    inline void SharedResourceMap::prune() {
        for (auto it = resources.cbegin(); it != resources.cend(); /* no increment */) {
            if (it->second.use_count() == 1) {
                resources.erase(it++);
            } else {
                it++;
            }
        }
    }

    inline typename SharedResourceMap::KeyViewType SharedResourceMap::keys() {
        return KeyViewType(resources);
    }

} // namespace elem
