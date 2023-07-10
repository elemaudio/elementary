#pragma once

#include "../GraphNode.h"
#include "../Invariant.h"
#include "../MultiChannelRingBuffer.h"

#include "helpers/Change.h"


namespace elem
{

    template <typename FloatType>
    struct CaptureNode : public GraphNode<FloatType> {
        CaptureNode(NodeId id, FloatType const sr, int const blockSize)
            : GraphNode<FloatType>::GraphNode(id, sr, blockSize),
              ringBuffer(1, 4 * blockSize)
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
            size_t writeStart = 0;
            size_t writeEnd = 0;

            for (size_t i = 0; i < numSamples; ++i) {
                auto const g = static_cast<bool>(inputData[0][i]);
                auto const dg = change(inputData[0][i]);

                // If we're at the rising edge of the gate signal, mark the write start
                if (dg > FloatType(0.5))
                    writeStart = i;

                // If we're recording, update the write end
                if (g && std::abs(dg) < FloatType(0.1))
                    writeEnd = i;

                // If we're at the falling edge of the gate signal, commit the recording
                if (dg < FloatType(-0.5)) {
                    ringBuffer.write(inputData, 1, writeEnd - writeStart);
                    relayReady.store(true);

                    writeStart = 0;
                    writeEnd = 0;
                }
            }

            if (writeEnd - writeStart > 0) {
                ringBuffer.write(inputData, 1, writeEnd - writeStart);
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
        std::vector<FloatType> relayBuffer;
        std::atomic<bool> relayReady = false;
    };

} // namespace elem
