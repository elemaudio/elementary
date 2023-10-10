#pragma once

#include "../GraphNode.h"
#include "../SingleWriterSingleReaderQueue.h"
#include "../Types.h"


namespace elem
{

    // TableNode is a core builtin for lookup table behaviors
    //
    // Can be used for loading sample buffers and reading from them at variable
    // playback rates, or as windowed grain readers, or for loading various functions
    // as lookup tables, etc.
    template <typename FloatType>
    struct TableNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val, SharedResourceMap<FloatType>& resources) override
        {
            if (key == "path") {
                if (!val.isString())
                    return ReturnCode::InvalidPropertyType();

                if (!resources.has((js::String) val))
                    return ReturnCode::InvalidPropertyValue();

                auto ref = resources.get((js::String) val);
                bufferQueue.push(std::move(ref));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // First order of business: grab the most recent sample buffer to use if
            // there's anything in the queue. This behavior means that changing the buffer
            // while playing the sample will cause a discontinuity.
            while (bufferQueue.size() > 0)
                bufferQueue.pop(activeBuffer);

            if (numChannels == 0 || activeBuffer == nullptr)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            auto const bufferSize = static_cast<int>(activeBuffer->size());
            auto const bufferData = activeBuffer->data();

            if (bufferSize == 0)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // Finally, render sample output
            for (size_t i = 0; i < numSamples; ++i) {
                auto const readPos = std::clamp(inputData[0][i], FloatType(0), FloatType(1)) * FloatType(bufferSize - 1);
                auto const readLeft = static_cast<int>(readPos);
                auto const readRight = readLeft + 1;
                auto const frac = readPos - std::floor(readPos);

                auto const left = bufferData[readLeft % bufferSize];
                auto const right = bufferData[readRight % bufferSize];

                // Now we can read the next sample out with linear
                // interpolation for sub-sample reads.
                outputData[i] = left + frac * (right - left);
            }
        }

        SingleWriterSingleReaderQueue<SharedResourceBuffer<FloatType>> bufferQueue;
        SharedResourceBuffer<FloatType> activeBuffer;
    };

} // namespace elem
