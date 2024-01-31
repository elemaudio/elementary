#pragma once

#include "../GraphNode.h"
#include "../MultiChannelRingBuffer.h"

#include "helpers/Change.h"
#include "helpers/BitUtils.h"


namespace elem
{

    template <typename FloatType>
    struct CaptureNode : public GraphNode<FloatType> {
        CaptureNode(NodeId id, FloatType const sr, int const blockSize)
            : GraphNode<FloatType>::GraphNode(id, sr, blockSize),
              ringBuffer(1, elem::bitceil(static_cast<size_t>(sr)))
        {
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
                auto const dg = change(inputData[0][i]);
                auto const fallingEdge = (dg < FloatType(-0.5));

                // If we're at the falling edge of the gate signal or need space in our scratch
                // we propagate the data into the ring
                if (fallingEdge || scratchSize >= scratchBuffer.size()) {
                    auto const* writeData = scratchBuffer.data();
                    ringBuffer.write(&writeData, 1, scratchSize);
                    scratchSize = 0;

                    // And if it's the falling edge we alert the event processor
                    if (fallingEdge) {
                        relayReady.store(true);
                    }
                }

                // Otherwise, if the record signal is on, write to our scratch
                if (g) {
                    scratchBuffer[scratchSize++] = inputData[1][i];
                }
            }
        }

        void processEvents(std::function<void(std::string const&, js::Value)>& eventHandler) override {
            auto const samplesAvailable = ringBuffer.size();

            if (samplesAvailable > 0) {
                auto currentSize = relayBuffer.size();
                relayBuffer.resize(currentSize + samplesAvailable);

                auto relayData = relayBuffer.data() + currentSize;

                if (!ringBuffer.read(&relayData, 1, samplesAvailable))
                    return;
            }

            if (relayReady.exchange(false)) {
                // Now we can go ahead and relay the data
                js::Float32Array copy;

                auto relaySize = relayBuffer.size();
                auto relayData = relayBuffer.data();

                copy.resize(relaySize);

                // Can't use std::copy_n here if FloatType is double, so we do it manually
                for (size_t i = 0; i < relaySize; ++i) {
                    copy[i] = static_cast<float>(relayData[i]);
                }

                relayBuffer.clear();

                eventHandler("capture", js::Object({
                    {"source", GraphNode<FloatType>::getPropertyWithDefault("name", js::Value())},
                    {"data", copy}
                }));
            }
        }

        Change<FloatType> change;
        MultiChannelRingBuffer<FloatType> ringBuffer;
        std::array<FloatType, 128> scratchBuffer;
        size_t scratchSize = 0;

        std::vector<FloatType> relayBuffer;
        std::atomic<bool> relayReady = false;
    };

} // namespace elem
