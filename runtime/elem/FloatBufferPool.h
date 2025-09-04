#include <memory>
#include <stack>
#include <unordered_map>
#include <utility>

#include "third-party/choc/choc/memory/choc_PoolAllocator.h"
#include "third-party/choc/choc/platform/choc_Assert.h"
#include "third-party/choc/choc/containers/choc_SmallVector.h"
#include "Types.h"

namespace elem
{

namespace detail
{

    struct AssignmentsMapKeyHash {
        template <typename T1, typename T2>
        std::size_t operator() (std::pair<T1, T2> const& p) const {
            auto h1 = std::hash<T1>{}(p.first);
            auto h2 = std::hash<T2>{}(p.second);

            h1 ^= h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2);
            return h1;
        }
    };

    // Returns the number of output channels given a set of outlet connections
    inline size_t getRequiredOutputChannels(std::vector<OutletConnection> const& outlets) {
        size_t numOuts = 1;

        for (auto const& connection : outlets) {
            // Outlet channels are zero indexed, hence the +1 for required channel count
            numOuts = std::max(numOuts, connection.outletChannel + 1);
        }

        return numOuts;
    }

    // Returns the number of connections made to the given channel, which is also the number
    // of times we expect the buffer contents to be consumed by another node.
    inline size_t getNumExpectedConsumers(std::vector<OutletConnection> const& outlets, size_t channel) {
        size_t c = 0;

        for (auto const& outlet : outlets) {
            if (outlet.outletChannel == channel) {
                c++;
            }
        }

        return c;
    }

} // namespace detail

template<typename FloatType>
using ChannelData = choc::SmallVector<FloatType*, 16>;

template<typename FloatType>
class FloatBufferPool {
public:
    FloatBufferPool(size_t blockSize);
    ~FloatBufferPool() = default;

    // Delete copy constructor and assignment
    FloatBufferPool(const FloatBufferPool&) = delete;
    FloatBufferPool& operator=(const FloatBufferPool&) = delete;

    // Provisions a set of float buffers for the give nodeId to write to
    // during the graph traversal.
    ChannelData<FloatType> produce(int32_t nodeId, std::vector<OutletConnection> const& outlets);

    // Provisions the set of float buffers for the give nodeId to read from
    // during the graph traversal.
    ChannelData<FloatType> consume(std::vector<InletConnection> const& inlets);

    // Peek at the allocated data for a given node/channel pair
    FloatType* peek(int32_t nodeId, size_t channel);

    // Reset internal state
    void clear();

private:
    // Gets a block of data from the free list or allocates new from the pool
    FloatType* getBlock();

    using AssignmentsMapKey = std::pair<int32_t, size_t>;
    struct AssignmentsMapValue {
        FloatType* buffer;
        size_t refCount;
    };

    std::unordered_map<AssignmentsMapKey, AssignmentsMapValue, detail::AssignmentsMapKeyHash> m_assignments;
    choc::memory::Pool m_memoryPool;
    std::stack<FloatType*> m_freeList;
    size_t m_blockSize;
};

// Implementation details...
template<typename FloatType>
inline FloatBufferPool<FloatType>::FloatBufferPool(size_t blockSize)
    : m_blockSize(blockSize)
{
}

template<typename FloatType>
inline ChannelData<FloatType> FloatBufferPool<FloatType>::produce(int32_t nodeId, std::vector<OutletConnection> const& outlets)
{
    ChannelData<FloatType> outChannels;

    auto const numOuts = detail::getRequiredOutputChannels(outlets);

    for (size_t i = 0; i < numOuts; ++i) {
        auto mapKey = std::pair(nodeId, i);
        auto* buffer = getBlock();
        auto refCount = detail::getNumExpectedConsumers(outlets, i);

        // We should never be producing for a given nodeId/channel pair more than once
        CHOC_ASSERT(m_assignments.count(mapKey) == 0);
        outChannels.push_back(buffer);
        m_assignments.emplace(mapKey, AssignmentsMapValue {
            buffer,
            refCount
        });
    }

    return std::move(outChannels);
}

template<typename FloatType>
inline ChannelData<FloatType> FloatBufferPool<FloatType>::consume(std::vector<InletConnection> const& inlets)
{
    ChannelData<FloatType> inChannels;

    for (auto const& inlet : inlets) {
        auto mapKey = std::pair(inlet.source, inlet.outletChannel);

        // If it's not in the map then we haven't yet visited a node who can produce
        // the output demanded by this inlet connection. That's a problem!
        CHOC_ASSERT(m_assignments.count(mapKey) > 0);

        auto& value = m_assignments.at(mapKey);
        inChannels.push_back(value.buffer);
        value.refCount--;

        if (value.refCount == 0) {
            m_freeList.push(value.buffer);
            m_assignments.erase(mapKey);
        }
    }

    return std::move(inChannels);
}

template<typename FloatType>
inline FloatType* FloatBufferPool<FloatType>::peek(int32_t nodeId, size_t channel)
{
    auto mapKey = std::pair(nodeId, channel);
    CHOC_ASSERT(m_assignments.count(mapKey) > 0);
    return m_assignments.at(mapKey).buffer;
}

template<typename FloatType>
inline void FloatBufferPool<FloatType>::clear()
{
    m_freeList = std::stack<FloatType*>();
    m_assignments.clear();
    m_memoryPool.reset();
}

template<typename FloatType>
inline FloatType* FloatBufferPool<FloatType>::getBlock() {
    if (m_freeList.empty()) {
        return static_cast<FloatType*>(m_memoryPool.allocateData(m_blockSize * sizeof(FloatType)));
    }

    auto* b = m_freeList.top();
    m_freeList.pop();
    return b;
}

} // namespace elem
