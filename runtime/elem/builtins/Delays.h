#pragma once

#include "../GraphNode.h"
#include "../SingleWriterSingleReaderQueue.h"

#include "helpers/Change.h"
#include "helpers/RefCountedPool.h"
#include "helpers/BitUtils.h"


namespace elem
{

    // A single sample delay node.
    template <typename FloatType>
    struct SingleSampleDelayNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            for (size_t i = 0; i < numSamples; ++i) {
                auto const x = inputData[0][i];

                outputData[i] = z;
                z = x;
            }
        }

        FloatType z = 0;
    };

    // A variable length delay line with a feedback path.
    //
    //   el.delay({size: 44100}, el.ms2samps(len), fb, x)
    //
    // A feedforward component can be added trivially:
    //
    //   el.add(el.mul(ff, x), el.delay({size: 44100}, el.ms2samps(len), fb, x))
    //
    // At small delay lengths and with a feedback or feedforward component, this becomes
    // a simple comb filter.
    template <typename FloatType>
    struct VariableDelayNode : public GraphNode<FloatType> {
        VariableDelayNode(NodeId id, FloatType const sr, int const blockSize)
            : GraphNode<FloatType>::GraphNode(id, sr, blockSize)
        {
            (void) setProperty("size", js::Value((js::Number) blockSize));
        }

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "size") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                auto const size = static_cast<int>((js::Number) val);
                auto data = bufferPool.allocate();

                // The buffer that we get from the pool may have been
                // previously used for a different delay buffer. Need to
                // resize here and then overwrite below.
                data->resize(size);

                for (size_t i = 0; i < data->size(); ++i)
                    data->at(i) = FloatType(0);

                // Finally, we push our new buffer into the event
                // queue for the realtime thread.
                bufferQueue.push(std::move(data));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // First order of business: grab the most recent delay buffer to use if
            // there's anything in the queue
            while (bufferQueue.size() > 0) {
                bufferQueue.pop(activeBuffer);
                writeIndex = 0;
            }

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 3)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            auto const size = static_cast<int>(activeBuffer->size());
            auto* delayData = activeBuffer->data();

            if (size == 0)
                return (void) std::copy_n(inputData[0], numSamples, outputData);

            for (size_t i = 0; i < numSamples; ++i) {
                auto const offset = std::clamp(inputData[0][i], FloatType(0), FloatType(size));

                // In order to get the feedback right for non-zero read offsets, we read from the
                // line and sum with the input before writing. If we have a read offset of zero, then
                // reading before writing doesn't work: we should be reading the same thing that we're
                // writing. In that case, feedback doesn't make much sense anyways, so here we have a
                // special case which ignores the feedback value and reads and writes correctly
                if (offset <= std::numeric_limits<FloatType>::epsilon()) {
                    auto const in = inputData[2][i];

                    delayData[writeIndex] = in;
                    outputData[i] = in;

                    // Nudge the write index
                    if (++writeIndex >= size)
                        writeIndex -= size;

                    continue;
                }

                auto const readFrac = FloatType(size + writeIndex) - offset;
                auto const readLeft = static_cast<int>(readFrac);
                auto const readRight = readLeft + 1;
                auto const frac = readFrac - std::floor(readFrac);

                // Because of summing `size` with `writeIndex` when forming
                // `readFrac` above, we know that we don't run our read pointers
                // below 0 ever, so here we only need to account for wrapping past
                // size which we do via modulo.
                auto const left = delayData[readLeft % size];
                auto const right = delayData[readRight % size];

                // Now we can read the next sample out of the delay line with linear
                // interpolation for sub-sample delays.
                auto const out = left + frac * (right - left);

                // Then we can form the input to the delay line with the feedback
                // component
                auto const fb = std::clamp(inputData[1][i], FloatType(-1), FloatType(1));
                auto const in = inputData[2][i] + fb * out;

                // Write to the delay line
                delayData[writeIndex] = in;

                // Write to the output buffer
                outputData[i] = out;

                // Nudge the write index
                if (++writeIndex >= size)
                    writeIndex -= size;
            }
        }

        using DelayBuffer = std::vector<FloatType>;

        RefCountedPool<DelayBuffer> bufferPool;
        SingleWriterSingleReaderQueue<std::shared_ptr<DelayBuffer>> bufferQueue;
        std::shared_ptr<DelayBuffer> activeBuffer;

        int writeIndex = 0;
    };

    // A simple fixed length, non-interpolated delay line.
    //
    //   el.sdelay({size: 44100}, x)
    //
    // Intended for use cases where `el.delay` is overkill, and `sdelay`
    // can provide helpful performance benefits.
    template <typename FloatType>
    struct SampleDelayNode : public GraphNode<FloatType> {
        SampleDelayNode(NodeId id, FloatType const sr, int const bs)
            : GraphNode<FloatType>::GraphNode(id, sr, bs)
            , blockSize(bs)
        {
            (void) setProperty("size", js::Value((js::Number) blockSize));
        }

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "size") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                // Here we make sure that we allocate at least one block size
                // more than the desired delay length so that we can run our write
                // pointer freely forward each block, never risking clobbering. Then
                // we round up to the next power of two for bit masking tricks around
                // the delay line length.
                auto const len = static_cast<int>((js::Number) val);
                auto const size = elem::bitceil(len + blockSize);
                auto data = bufferPool.allocate();

                // The buffer that we get from the pool may have been
                // previously used for a different delay buffer. Need to
                // resize here and then overwrite below.
                data->resize(size);

                for (size_t i = 0; i < data->size(); ++i)
                    data->at(i) = FloatType(0);

                // Finally, we push our new buffer into the event
                // queue for the realtime thread.
                bufferQueue.push(std::move(data));
                length.store(len);
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // First order of business: grab the most recent delay buffer to use if
            // there's anything in the queue
            while (bufferQueue.size() > 0) {
                bufferQueue.pop(activeBuffer);
                writeIndex = 0;
            }

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1 || activeBuffer == nullptr)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            auto const size = static_cast<int>(activeBuffer->size());
            auto const mask = size - 1;
            auto const len = length.load();

            if (size == 0)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            auto* delayData = activeBuffer->data();
            auto* inData = inputData[0];

            auto readStart = writeIndex - len;

            for (size_t i = 0; i < numSamples; ++i) {
                // Write to the delay line
                auto const in = inData[i];
                delayData[writeIndex] = in;

                // Nudge the write index
                writeIndex = (writeIndex + 1) & mask;
            }

            for (int i = 0; i < static_cast<int>(numSamples); ++i) {
                // Write to the output buffer
                outputData[i] = delayData[(size + readStart + i) & mask];
            }
        }

        using DelayBuffer = std::vector<FloatType>;

        RefCountedPool<DelayBuffer> bufferPool;
        SingleWriterSingleReaderQueue<std::shared_ptr<DelayBuffer>> bufferQueue;
        std::shared_ptr<DelayBuffer> activeBuffer;

        std::atomic<int> length = 0;
        int writeIndex = 0;
        int blockSize = 0;
    };

} // namespace elem
