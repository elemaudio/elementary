#pragma once

#include <functional>
#include <memory>
#include <set>
#include <stack>
#include <unordered_map>
#include <utility>

#include "BlockEvents.h"
#include "third-party/choc/choc/memory/choc_PoolAllocator.h"
#include "third-party/choc/choc/platform/choc_Assert.h"
#include "Types.h"

namespace elem
{

namespace detail
{

    inline std::set<NodeId> getDistinctNodeIds(std::vector<OutletConnection> const& outlets) {
        std::set<NodeId> s;

        for (auto const& outlet : outlets) {
            s.insert(outlet.destination);
        }

        return s;
    }

    inline std::set<NodeId> getDistinctNodeIds(std::vector<InletConnection> const& inlets) {
        std::set<NodeId> s;

        for (auto const& inlet : inlets) {
            s.insert(inlet.source);
        }

        return s;
    }


} // namespace detail

class BlockEventsBufferPool {
public:
    BlockEventsBufferPool() = default;
    ~BlockEventsBufferPool() = default;

    // Delete copy constructor and assignment
    BlockEventsBufferPool(const BlockEventsBufferPool&) = delete;
    BlockEventsBufferPool& operator=(const BlockEventsBufferPool&) = delete;

    // Provisions a BlockEvents struct for the given nodeId to write to
    // during the graph traversal
    BlockEvents& produce(int32_t nodeId, std::vector<OutletConnection> const& outlets);

    // Provisions the BlockEvents struct for the give nodeId to read from
    // during the graph traversal.
    //
    // If more than one inlet produces block events, this will provision a new
    // struct from the pool, merge the events into one, and return that.
    BlockEvents& consume(std::vector<InletConnection> const& inlets);

    // Reset internal state
    void clear();

private:
    // Gets a block of data from the free list or allocates new from the pool
    BlockEvents& getEventsBuffer();

    struct AssignmentsMapValue {
        BlockEvents& buffer;
        size_t refCount;
    };

    std::unordered_map<NodeId, AssignmentsMapValue> m_assignments;
    choc::memory::Pool m_memoryPool;
    std::stack<std::reference_wrapper<BlockEvents>> m_freeList;
};

// Implementation details...
inline BlockEvents& BlockEventsBufferPool::produce(int32_t nodeId, std::vector<OutletConnection> const& outlets)
{
    // We should never be producing for a given nodeId more than once
    CHOC_ASSERT(m_assignments.count(nodeId) == 0);
    auto refCount = detail::getDistinctNodeIds(outlets).size();
    auto& buffer = getEventsBuffer();

    m_assignments.emplace(nodeId, AssignmentsMapValue {
        buffer,
        refCount
    });

    return buffer;
}

inline BlockEvents& BlockEventsBufferPool::consume(std::vector<InletConnection> const& inlets)
{
    auto childIds = detail::getDistinctNodeIds(inlets);

    // We shouldn't be trying to consume from the pool if the node has no children, instead
    // the rendering algorithm should pick the host events
    CHOC_ASSERT(childIds.size() > 0);

    // If there's just one child we can just pluck its buffer from the map
    if (childIds.size() == 1) {
        auto childId = *childIds.begin();
        CHOC_ASSERT(m_assignments.count(childId) > 0);

        auto& value = m_assignments.at(childId);
        auto& buffer = value.buffer;
        value.refCount--;

        if (value.refCount == 0) {
            m_freeList.push(value.buffer);
            m_assignments.erase(childId);
        }

        return buffer;
    }

    // Else, we take a new temporary block and sum into it
    //
    // We can just put the buffer immediately back in the free list
    // because this node is the only consumer of this temporary block
    auto& buffer = getEventsBuffer();
    m_freeList.push(buffer);

    // TODO: Consume each child id and sum into `buffer` before returning
    return buffer;
}

inline void BlockEventsBufferPool::clear()
{
    m_freeList = std::stack<std::reference_wrapper<BlockEvents>>();
    m_assignments.clear();
    m_memoryPool.reset();
}

inline BlockEvents& BlockEventsBufferPool::getEventsBuffer() {
    if (m_freeList.empty()) {
        return m_memoryPool.allocate<BlockEvents>();
    }

    auto& b = m_freeList.top();
    m_freeList.pop();
    return b;
}

} // namespace elem
