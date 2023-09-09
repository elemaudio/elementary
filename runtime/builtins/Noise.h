#pragma once

#include "../GraphNode.h"


namespace elem
{

    template <typename FloatType>
    struct UniformRandomNoiseNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "seed") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                seed = static_cast<uint32_t>((js::Number) val);
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        // Neat random number generator found here: https://stackoverflow.com/a/3747462/2320243
        // Turns out it's almost exactly the same one used in Max/MSP's Gen for its [noise] block
        inline int fastRand()
        {
            seed = 214013 * seed + 2531011;
            return (seed >> 16) & 0x7FFF;
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto* outputData = ctx.outputData;
            auto numSamples = ctx.numSamples;

            for (size_t i = 0; i < numSamples; ++i) {
                outputData[i] = (fastRand() / static_cast<FloatType>(0x7FFF));
            }
        }

        uint32_t seed = static_cast<uint32_t>(std::rand());
    };

} // namespace elem
