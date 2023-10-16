#pragma once

#include <elem/GraphNode.h>


namespace elem
{

    // A simple node which just emits the current sample time as a continuous
    // signal.
    template <typename FloatType>
    struct SampleTimeNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto* outputData = ctx.outputData;
            auto numSamples = ctx.numSamples;
            auto sampleTime = *static_cast<int64_t*>(ctx.userData);

            for (size_t i = 0; i < numSamples; ++i) {
                outputData[i] = static_cast<double>(sampleTime + i);
            }
        }
    };

} // namespace elem
