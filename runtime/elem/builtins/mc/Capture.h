#pragma once

#include "../../GraphNode.h"
#include "../../MultiChannelRingBuffer.h"
#include "../../Types.h"

#include "../helpers/Change.h"
#include "../helpers/BitUtils.h"


namespace elem
{

    template <typename FloatType>
    struct MCCaptureNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "_internal:numChildren") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                auto numChildren = (js::Number) val;

                if (numChildren > 1.0) {
                    auto numCaptureChans = static_cast<size_t>((js::Number) val) - 1;
                    ringBuffer = std::make_unique<MultiChannelRingBuffer<FloatType>>(numCaptureChans, elem::bitceil(static_cast<int>(GraphNode<FloatType>::getSampleRate())));
                    relayBuffer.resize(numCaptureChans, 8192);
                }
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto** outputData = ctx.outputData;
            auto numIns = ctx.numInputChannels;
            auto numOuts = ctx.numOutputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numIns < 2) {
                for (size_t i = 0; i < numOuts; ++i) {
                    std::fill_n(outputData[i], numSamples, FloatType(0));
                }

                return;
            }

            // Propagate input
            for (size_t i = 0; i < numOuts; ++i) {
                // If we have an input channel to propagate, we do
                if (i + 1 < numIns) {
                    std::copy_n(inputData[i + 1], numSamples, outputData[i]);
                    continue;
                }

                // Else we zero
                std::fill_n(outputData[i], numSamples, FloatType(0));
            }

            // Capture
            size_t writeStart = 0;
            size_t writeStop = 0;

            for (size_t i = 0; i < numSamples; ++i) {
                auto const g = static_cast<bool>(inputData[0][i]);
                auto const dg = change(inputData[0][i]);
                auto const fallingEdge = (dg < FloatType(-0.5));
                auto const risingEdge = (dg > FloatType(0.5));

                // If we're at the falling edge of the gate signal we propagate
                // the data into the ring and signal ready
                if (fallingEdge) {
                    if (ringBuffer) {
                        ringBuffer->write(inputData + 1, numIns - 1, writeStop - writeStart, writeStart);
                    }

                    relayReady.store(true);
                    writeStart = 0;
                    writeStop = 0;
                }

                // Otherwise, if the record signal is on, we update our capture markers
                if (g) {
                    // Stop on the next sample
                    writeStop = i + 1;

                    if (risingEdge) {
                        writeStart = i;
                    }
                }
            }

            if (ringBuffer && (writeStop - writeStart) > 0) {
                ringBuffer->write(inputData + 1, numIns - 1, writeStop - writeStart, writeStart);
            }
        }

        void processEvents(std::function<void(std::string const&, js::Value)>& eventHandler) override {
            auto samplesAvailable = static_cast<int>(ringBuffer->size());

            while (ringBuffer && samplesAvailable > 0) {
                relayBuffer.clear();

                auto** relayData = relayBuffer.data();
                auto numChansToRead = std::min(ringBuffer->getNumChannels(), relayBuffer.getNumChannels());
                auto numSamplesToRead = std::min(samplesAvailable, static_cast<int>(relayBuffer.getNumSamples()));

                if (!ringBuffer->read(relayData, numChansToRead, numSamplesToRead)) {
                    return;
                }

                samplesAvailable -= numSamplesToRead;

                // Push to pending event object
                pendingEventData.resize(numChansToRead, js::Float32Array());

                for (size_t i = 0; i < numChansToRead; ++i) {
                    auto& channelCopy = pendingEventData[i].getFloat32Array();
                    auto currentSize = channelCopy.size();

                    channelCopy.resize(currentSize + numSamplesToRead);

                    // Can't use std::copy_n here if FloatType is double, so we do it manually
                    for (size_t j = 0; j < numSamplesToRead; ++j) {
                        channelCopy[currentSize + j] = static_cast<float>(relayData[i][j]);
                    }
                }
            }

            // Here we can go ahead and relay the data
            if (relayReady.exchange(false)) {
                eventHandler("mc.capture", js::Object({
                    {"source", GraphNode<FloatType>::getPropertyWithDefault("name", js::Value())},
                    {"data", pendingEventData}
                }));

                for (auto& chan : pendingEventData) {
                    chan.getFloat32Array().clear();
                }
            }
        }

        std::unique_ptr<MultiChannelRingBuffer<FloatType>> ringBuffer;
        AudioBuffer<FloatType> relayBuffer;
        std::atomic<bool> relayReady = false;
        js::Array pendingEventData;

        Change<FloatType> change;
        bool capturing = false;
    };

} // namespace elem
