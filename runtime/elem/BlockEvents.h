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

    MidiEvent(uint8_t byte0, uint8_t byte1, uint8_t byte2)
        : message(byte0, byte1, byte2)
    {}
};

// Type-erased event structure that can hold any event type within a certain
// size. The event struct holds its data strictly in stack-allocated space.
struct BlockEvent {
    size_t time;

    static constexpr size_t kMaxObjectSize = 64;
    alignas(std::max_align_t) char data[kMaxObjectSize];
    std::type_index typeIndex;

    template <typename T>
    BlockEvent(size_t t, T&& d)
        : time(t)
        , typeIndex(std::type_index(typeid(T)))
    {
        static_assert(sizeof(T) <= kMaxObjectSize, "Type too large for BlockEvent buffer");
        static_assert(alignof(T) <= alignof(std::max_align_t), "Type alignment too strict");
        static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");

        new(data) T(std::forward<T>(d));
    }

    template <typename T>
    T* get_if() {
        if (std::type_index(typeid(T)) == typeIndex) {
            return reinterpret_cast<T*>(&data);
        }

        return nullptr;
    }

    template <typename T>
    T const* get_if() const {
        if (std::type_index(typeid(T)) == typeIndex) {
            return reinterpret_cast<T const*>(&data);
        }

        return nullptr;
    }

    bool operator>(const BlockEvent& other) const {
        return time > other.time;
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
    inline void processEventsOfType(Handler&& handler) const {
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

    // Sort events by time
    inline void sort() {
        auto size = storage.size();

        for (size_t i = 1; i < size; ++i) {
            auto key = storage[i];  // Element to be inserted
            int j = i - 1;   // Start comparing with the element before

            // Move elements of storage[0..i-1] that are greater than key
            // one position ahead of their current position
            while (j >= 0 && storage[j] > key) {
                storage[j + 1] = storage[j];  // Shift element to the right
                j--;                   // Move to the next element on the left
            }

            // Insert the key at its correct position
            storage[j + 1] = key;
        }
    }
};

} // namespace elem
