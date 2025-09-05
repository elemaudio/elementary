#pragma once

#include <any>
#include <typeindex>

#include "third-party/choc/choc/containers/choc_SmallVector.h"
#include "third-party/choc/choc/audio/choc_MIDI.h"


namespace elem
{

struct ParamValueEvent {
    size_t paramIndex;
    float value;
};

struct MidiEvent {
    choc::midi::ShortMessage message;
};

struct AssignedMidiEvent {
    choc::midi::ShortMessage message;
    size_t voiceIndex;
};

// Type-erased event structure that can hold any event type
struct BlockEvent {
    size_t time;
    // TODO: Could allocate on large types?? Convert to a stack managed
    // pattern here...
    std::any data;

    template <typename T>
    BlockEvent(size_t t, T&& d)
        : time(t)
        , data(std::forward<T>(d))
    {}

    template <typename T>
    T* get_if() {
        if (std::type_index(data.type()) == std::type_index(typeid(T))) {
            return std::any_cast<T>(&data);
        }

        return nullptr;
    }

    template <typename T>
    T const* get_if() const {
        if (std::type_index(data.type()) == std::type_index(typeid(T))) {
            return std::any_cast<T>(&data);
        }

        return nullptr;
    }
};

// A buffer of realtime BlockEvent instances
//
// By default we have enough memory for 128 events, if you push more than
// that this struct will allocate. In those cases you can construct off the
// realtime thread, reserve capacity, and then use the container on the realtime
// thread with more headroom.
struct BlockEvents {
    choc::SmallVector<BlockEvent, 128> storage;

    // Helper for adding events
    template <typename T>
    inline void addEvent(size_t time, T&& data) {
        storage.emplace_back(time, std::forward<T>(data));
    }

    // Helper to process events of a specific type
    template <typename T, typename Handler>
    inline void processEventsOfType(const BlockEvents& events, Handler&& handler) {
        for (auto const& event : storage) {
            if (auto* data = event.get_if<T>()) {
                handler(event.time, *data);
            }
        }
    }

    // Reset the internal storage
    inline void clear() {
        storage.clear();
    }
};

} // namespace elem
