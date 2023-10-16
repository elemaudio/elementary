#pragma once

#include "ElemAssert.h"


namespace elem
{

    //==============================================================================
    // This is a simple lock-free single producer, single consumer queue for
    // buffered multichannel sample data.
    template <typename T>
    class MultiChannelRingBuffer
    {
    public:
        //==============================================================================
        MultiChannelRingBuffer(size_t numChannels, size_t capacity = 8192)
            : maxElements(capacity), indexMask(capacity - 1)
        {
            // We need the queue length to be non-zero and a power of two so
            // that our bit masking trick will work. Enforce that here with
            // an assert.
            ELEM_ASSERT(capacity > 0 && ((capacity & indexMask) == 0));
            ELEM_ASSERT(numChannels > 0);

            for (size_t i = 0; i < numChannels; ++i) {
                buffers.push_back(std::vector<T>(capacity));
            }
        }

        ~MultiChannelRingBuffer() = default;

        //==============================================================================
        void write (T const** data, size_t numChannels, size_t numSamples)
        {
            auto const w = writePos.load();
            auto const r = readPos.load();

            // If we're asked to write more than we have room for, we clobber and
            // nudge the read pointer up
            bool const shouldMoveReadPointer = (numSamples >= numFreeSlots(r, w));
            auto const desiredWritePosition = (w + numSamples) & indexMask;
            auto const desiredReadPosition = shouldMoveReadPointer ? ((desiredWritePosition + 1) & indexMask) : r;

            for (size_t i = 0; i < std::min(buffers.size(), numChannels); ++i) {
                if (w + numSamples >= maxElements) {
                    auto const s1 = maxElements - w;
                    std::copy_n(data[i], s1, buffers[i].data() + w);
                    std::copy_n(data[i] + s1, numSamples - s1, buffers[i].data());
                } else {
                    std::copy_n(data[i], numSamples, buffers[i].data() + w);
                }
            }

            writePos.store(desiredWritePosition);
            readPos.store(desiredReadPosition);
        }

        bool read (T** destination, size_t numChannels, size_t numSamples)
        {
            auto const r = readPos.load();
            auto const w = writePos.load();

            if (numFullSlots(r, w) >= numSamples)
            {
                for (size_t i = 0; i < std::min(buffers.size(), numChannels); ++i) {
                    if (r + numSamples >= maxElements) {
                        auto const s1 = maxElements - r;
                        std::copy_n(buffers[i].data() + r, s1, destination[i]);
                        std::copy_n(buffers[i].data(), numSamples - s1, destination[i] + s1);
                    } else {
                        std::copy_n(buffers[i].data() + r, numSamples, destination[i]);
                    }
                }

                auto const desiredReadPosition = (r + numSamples) & indexMask;
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
        std::vector<std::vector<T>> buffers;

        size_t indexMask = 0;
    };

} // namespace elem

