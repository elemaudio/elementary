#pragma once

#include <optional>

#include "ElemAssert.h"


namespace elem
{

    //==============================================================================
    // This is a simple lock-free single producer, single consumer queue.
    template <typename T>
    class SingleWriterSingleReaderQueue
    {
    public:
        //==============================================================================
        SingleWriterSingleReaderQueue(size_t capacity = 32)
            : maxElements(capacity), indexMask(capacity - 1)
        {
            // We need the queue length to be non-zero and a power of two so
            // that our bit masking trick will work. Enforce that here with
            // an assert.
            ELEM_ASSERT(capacity > 0 && ((capacity & indexMask) == 0));
            queue.resize(capacity);
        }

        ~SingleWriterSingleReaderQueue() = default;

        //==============================================================================
        bool push (T && el)
        {
            auto const w = writePos.load();
            auto const r = readPos.load();

            if (numFreeSlots(r, w) > 0)
            {
                queue[w] = std::move(el);
                auto const desiredWritePosition = (w + 1u) & indexMask;
                writePos.store(desiredWritePosition);
                return true;
            }

            return false;
        }

        bool push (std::vector<T>& els)
        {
            auto const w = writePos.load();
            auto const r = readPos.load();

            if (els.size() >= numFreeSlots(r, w))
                return false;

            for (size_t i = 0; i < els.size(); ++i)
                queue[(w + i) & indexMask] = std::move(els[i]);

            auto const desiredWritePosition = (w + els.size()) & indexMask;
            writePos.store(desiredWritePosition);
            return true;
        }

        bool pop (T& el)
        {
            auto const r = readPos.load();
            auto const w = writePos.load();

            if (numFullSlots(r, w) > 0)
            {
                el = std::move(queue[r]);
                auto const desiredReadPosition = (r + 1u) & indexMask;
                readPos.store(desiredReadPosition);
                return true;
            }

            return false;
        }

        size_t size()
        {
            auto const r = readPos.load();
            auto const w = writePos.load();

            return numFullSlots(r, w);
        }

    private:
        size_t numFullSlots(size_t const r, size_t const w)
        {
            // If the writer is ahead of the reader, then the full slots are
            // the ones ahead of the reader and behind the writer.
            if (w > r)
                return w - r;

            // Else, the writer has already wrapped around, so the free space is
            // what's ahead of the writer and behind the reader, and the full space
            // is what's left.
            return (maxElements - (r - w)) & indexMask;
        }

        size_t numFreeSlots(size_t const r, size_t const w)
        {
            // If the reader is ahead of the writer, then the writer must have
            // wrapped around already, so the only space available is whats ahead
            // of the writer, behind the reader.
            if (r > w)
                return r - w;

            // Else, the only full slots are ahead of the reader and behind the
            // writer, so the free slots are what's left.
            return maxElements - (w - r);
        }

        std::atomic<size_t> maxElements = 0;
        std::atomic<size_t> readPos = 0;
        std::atomic<size_t> writePos = 0;
        std::vector<T> queue;

        size_t indexMask = 0;
    };

} // namespace elem

