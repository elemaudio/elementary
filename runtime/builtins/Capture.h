#pragma once

#include <atomic>
#include <memory>

#include "../GraphNode.h"
#include "../Invariant.h"
#include "../SingleWriterSingleReaderQueue.h"

#include "helpers/RefCountedPool.h"


namespace elem
{

    template <typename FloatType>
    struct CaptureNode : public GraphNode<FloatType> {
        CaptureNode(NodeId id, FloatType const sr, int const blockSize)
            : GraphNode<FloatType>::GraphNode(id, sr, blockSize)
        {
            setProperty("size", js::Value((js::Number) blockSize));
        }

        void setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "size") {
                invariant(val.isNumber(), "size prop for loopcap node must be a number type.");

                auto size = static_cast<size_t>((js::Number) val);

                // First drop any buffer references we're holding here on the main thread
                std::atomic_store(&relay, std::shared_ptr<BufferType>(nullptr));

                // Next we go resize everything that we can
                bufferPool.forEach([&](std::shared_ptr<BufferType>& buffer) {
                    if (buffer.use_count() == 1) {
                        buffer->resize(size);
                    }
                });
            }

            GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 2)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // Propagate input
            std::copy_n(inputData[1], numSamples, outputData);

            // Capture
            for (size_t i = 0; i < numSamples; ++i) {
                auto const g = static_cast<bool>(inputData[0][i]);
                auto const xn = inputData[1][i];

                // If the record signal is low and there's nothing to relay, skip ahead
                if (!g && captureBuffer == nullptr)
                    continue;

                // If the record signal is low and we have something to relay, do it
                if (!g && captureBuffer != nullptr) {
                    std::atomic_store(&relay, std::move(captureBuffer));
                    captureBuffer = nullptr;
                    continue;
                }

                // If the record signal is high, we try to capture, grabbing a new buffer
                // to record into if we don't yet have one
                if (g) {
                    if (captureBuffer == nullptr) {
                        captureBuffer = bufferPool.allocateAvailableWithDefault(nullptr);

                        // If captureBuffer is still nullptr, the allocation failed, and there's
                        // nothing we can do right now
                        if (captureBuffer == nullptr)
                            continue;

                        // Else, we reset our write pos and carry on
                        writePos = 0;
                    }

                    if (writePos < captureBuffer->size()) {
                        auto* captureData = captureBuffer->data();
                        captureData[writePos++] = xn;
                    }
                }
            }
        }

        void processEvents(std::function<void(std::string const&, js::Value)>& eventHandler) override {
            js::Float32Array copy;

            if (auto relayBuffer = std::atomic_exchange(&relay, std::shared_ptr<BufferType>(nullptr)))
            {
                auto sizeProp = GraphNode<FloatType>::getPropertyWithDefault("size", (js::Number) 0);
                auto expectedSize = static_cast<size_t>(sizeProp);

                // If we land in here, it's likely because we got a new size prop in setProperty but
                // couldn't resize the buffer that the realtime thread was holding at the time. We're
                // now looking at that buffer and it doesn't match the expected size, so we resize it
                // and skip the relay
                if (expectedSize > 0 && relayBuffer->size() != expectedSize) {
                    relayBuffer->resize(expectedSize);
                    return;
                }

                // Now we can go ahead and relay the data
                auto relaySize = relayBuffer->size();
                auto relayData = relayBuffer->data();

                copy.resize(relaySize);

                // Can't use std::copy_n here if FloatType is double, so we do it manually
                for (size_t i = 0; i < relaySize; ++i) {
                    copy[i] = static_cast<float>(relayData[i]);
                }

                eventHandler("capture", js::Object({
                    {"source", GraphNode<FloatType>::getPropertyWithDefault("name", js::Value())},
                    {"data", copy}
                }));
            }
        }

        using BufferType = std::vector<FloatType>;
        RefCountedPool<BufferType> bufferPool;

        std::shared_ptr<BufferType> captureBuffer;
        size_t writePos = 0;

        std::shared_ptr<BufferType> relay;
    };

} // namespace elem
