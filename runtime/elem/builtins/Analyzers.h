#pragma once

#include <atomic>

#include "../GraphNode.h"
#include "../SingleWriterSingleReaderQueue.h"
#include "../MultiChannelRingBuffer.h"


namespace elem
{

    // A simple value metering node.
    //
    // Will pass its input through unaffected, but report the maximum and minimum
    // value seen through its event relay.
    //
    // Helpful for metering audio streams for things like drawing gain meters.
    template <typename FloatType>
    struct MeterNode : public GraphNode<FloatType> {
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

            // Copy input to output
            std::copy_n(inputData[0], numSamples, outputData);

            // Report the maximum value this block
            auto const [min, max] = std::minmax_element(inputData[0], inputData[0] + numSamples);
            (void) readoutQueue.push({ *min, *max });
        }

        void processEvents(std::function<void(std::string const&, js::Value)>& eventHandler) override {
            if (readoutQueue.size() <= 0)
                return;

            // Clear the readoutQueue into a local struct here. This way the readout
            // we propagate is the latest one and empties the queue for the processing thread
            ValueReadout ro;

            while (readoutQueue.size() > 0) {
                if (!readoutQueue.pop(ro)) {
                    return;
                }
            }

            // Now we propagate the latest value
            eventHandler("meter", js::Object({
                {"min", ro.min},
                {"max", ro.max},
                {"source", GraphNode<FloatType>::getPropertyWithDefault("name", js::Value())},
            }));
        }

        struct ValueReadout {
            FloatType min = 0;
            FloatType max = 0;
        };

        SingleWriterSingleReaderQueue<ValueReadout> readoutQueue;
    };

    // A simple value metering node whose callback is triggered on the rising
    // edge of an incoming pulse train.
    //
    // Will pass its input through unaffected, but report the value captured at
    // exactly the time of the rising edge.
    template <typename FloatType>
    struct SnapshotNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // We need the first channel to be the control signal and the second channel to
            // represent the signal we want to sample when the control changes
            if (numChannels < 2)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            for (size_t i = 0; i < numSamples; ++i) {
                const FloatType l = inputData[0][i];
                const FloatType x = inputData[1][i];
                const FloatType eps = std::numeric_limits<FloatType>::epsilon();

                // Check if it's time to latch
                // We're looking for L[i-1] near zero and L[i] non-zero, where L
                // is the discrete clock signal used to identify the hold. We latch
                // on the positive-going transition from a zero value to a non-zero
                // value.
                if (std::abs(z) <= eps && l > eps) {
                    (void) readoutQueue.push({ x });
                }

                z = l;
                outputData[i] = x;
            }
        }

        void processEvents(std::function<void(std::string const&, js::Value)>& eventHandler) override {
            // Now we propagate the latest value if we have one.
            if (readoutQueue.size() <= 0)
                return;

            // Clear the readoutQueue into a local struct here. This way the readout
            // we propagate is the latest one and empties the queue for the processing thread
            ValueReadout ro;

            while (readoutQueue.size() > 0) {
                if (!readoutQueue.pop(ro)) {
                    return;
                }
            }

            eventHandler("snapshot", js::Object({
                {"source", GraphNode<FloatType>::getPropertyWithDefault("name", js::Value())},
                {"data", ro.val},
            }));
        }

        struct ValueReadout {
            FloatType val = 0;
        };

        SingleWriterSingleReaderQueue<ValueReadout> readoutQueue;
        FloatType z = 0;
    };

    // An oscilloscope node which reports Float32Array buffers through
    // the event processing interface.
    //
    // Expecting potentially n > 1 children, will assemble an array of arrays representing each
    // child's buffer, so that we have coordinated streams
    template <typename FloatType>
    struct ScopeNode : public GraphNode<FloatType> {
        ScopeNode(NodeId id, FloatType const sr, int const blockSize)
            : GraphNode<FloatType>::GraphNode(id, sr, blockSize)
            , ringBuffer(4)
        {
            setProperty("channels", js::Value((js::Number) 1));
            setProperty("size", js::Value((js::Number) 512));
        }

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "size") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                if ((js::Number) val < 256 || (js::Number) val > 8192)
                    return ReturnCode::InvalidPropertyValue();
            }

            if (key == "channels") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                if ((js::Number) val < 0 || (js::Number) val > 4)
                    return ReturnCode::InvalidPropertyValue();
            }

            if (key == "name") {
                if (!val.isString())
                    return ReturnCode::InvalidPropertyType();
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // Copy input to output
            std::copy_n(inputData[0], numSamples, outputData);

            // Fill our ring buffer
            ringBuffer.write(inputData, numChannels, numSamples);
        }

        void processEvents(std::function<void(std::string const&, js::Value)>& eventHandler) override {
            auto const size = static_cast<size_t>((js::Number) GraphNode<FloatType>::getPropertyWithDefault("size", js::Number(512)));
            auto const channels = static_cast<size_t>((js::Number) GraphNode<FloatType>::getPropertyWithDefault("channels", js::Number(1)));

            if (ringBuffer.size() > size) {
                // Retreive the scope data. Could improve the efficiency here using something
                // like a pmr pool allocator so that on each call to processEvents here we're
                // not actually allocating and deallocating new memory
                js::Array scopeData (channels);

                // If our ScopeNode instance is templated on `float` then we can read from the
                // ring buffer directly into a js::Float32Array value type
                if constexpr (std::is_same_v<FloatType, float>) {
                    for (size_t i = 0; i < channels; ++i) {
                        scopeData[i] = js::Float32Array(size);
                        scratchPointers[i] = scopeData[i].getFloat32Array().data();
                    }

                    if (ringBuffer.read(scratchPointers.data(), channels, size)) {
                        eventHandler("scope", js::Object({
                            {"source", GraphNode<FloatType>::getPropertyWithDefault("name", js::Value())},
                            {"data", std::move(scopeData)}
                        }));
                    }
                }

                // If our ScopeNode instance is templated on `double` then we read into a temporary
                // vector of doubles before copying (and casting) to a js::Float32Array value
                if constexpr (std::is_same_v<FloatType, double>) {
                    std::vector<double> temp(channels * size);

                    for (size_t i = 0; i < channels; ++i) {
                        scopeData[i] = js::Float32Array(size);
                        scratchPointers[i] = temp.data() + (i * size);
                    }

                    if (ringBuffer.read(scratchPointers.data(), channels, size)) {
                        for (size_t i = 0; i < channels; ++i) {
                            auto* dest = scopeData[i].getFloat32Array().data();

                            for (size_t j = 0; j < size; ++j) {
                                dest[j] = static_cast<float>(temp[(i * size) + j]);
                            }
                        }

                        eventHandler("scope", js::Object({
                            {"source", GraphNode<FloatType>::getPropertyWithDefault("name", js::Value())},
                            {"data", std::move(scopeData)}
                        }));
                    }
                }
            }
        }

        std::array<FloatType*, 8> scratchPointers;
        MultiChannelRingBuffer<FloatType> ringBuffer;
    };

} // namespace elem
