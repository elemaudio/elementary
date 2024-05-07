#pragma once

#include "../../GraphNode.h"
#include "../../SingleWriterSingleReaderQueue.h"
#include "../../Types.h"


namespace elem
{

    template <typename FloatType>
    struct StereoTableNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        size_t getNumOutputChannels() override
        {
            return 2;
        }

        int setProperty(std::string const& key, js::Value const& val, SharedResourceMap& resources) override
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
            auto** outputData = ctx.outputData;
            auto numChannels = ctx.numOutputChannels;
            auto numSamples = ctx.numSamples;

            // First order of business: grab the most recent sample buffer to use if
            // there's anything in the queue. This behavior means that changing the buffer
            // while playing the sample will cause a discontinuity.
            while (bufferQueue.size() > 0)
                bufferQueue.pop(activeBuffer);

            if (ctx.numInputChannels == 0 || activeBuffer == nullptr)
            {
                for (size_t j = 0; j < numChannels; ++j)
                {
                    std::fill_n(outputData[j], numSamples, FloatType(0));
                }

                return;
            }

            for (size_t j = 0; j < numChannels; ++j)
            {
                auto const bufferView = activeBuffer->getChannelData(j);
                auto const bufferSize = bufferView.size();
                auto const bufferData = bufferView.data();
                auto* inputData = ctx.inputData[0];


                if (bufferSize == 0)
                {
                    std::fill_n(outputData[j], numSamples, FloatType(0));
                    continue;
                }

                for (size_t i = 0; i < numSamples; ++i)
                {
                    auto const readPos = std::clamp(inputData[i], FloatType(0), FloatType(1)) * FloatType(bufferSize - 1);
                    auto const readLeft = static_cast<size_t>(readPos);
                    auto const readRight = readLeft + 1;
                    auto const frac = readPos - std::floor(readPos);

                    auto const left = bufferData[readLeft % bufferSize];
                    auto const right = bufferData[readRight % bufferSize];

                    // Now we can read the next sample out with linear
                    // interpolation for sub-sample reads.
                    outputData[j][i] = left + frac * (right - left);
                }
            }
        }

        SingleWriterSingleReaderQueue<SharedResourcePtr> bufferQueue;
        SharedResourcePtr activeBuffer;
    };

} // namespace elem
