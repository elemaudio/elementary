#pragma once

#include "../GraphNode.h"


namespace elem
{

    namespace detail
    {

        enum class BlepMode {
            Saw = 0,
            Square = 1,
            Triangle = 2,
        };
    }

    template <typename FloatType, detail::BlepMode Mode>
    struct PolyBlepOscillatorNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        inline FloatType blep (FloatType phase, FloatType increment)
        {
            if (phase < increment)
            {
                auto p = phase / increment;
                return (FloatType(2) - p) * p - FloatType(1);
            }

            if (phase > (FloatType(1) - increment))
            {
                auto p = (phase - FloatType(1)) / increment;
                return (p + FloatType(2)) * p + FloatType(1);
            }

            return FloatType(0);
        }

        inline FloatType tick (FloatType phase, FloatType increment)
        {
            if constexpr (Mode == detail::BlepMode::Saw) {
                return FloatType(2) * phase - FloatType(1) - blep(phase, increment);
            }

            if constexpr (Mode == detail::BlepMode::Square || Mode == detail::BlepMode::Triangle) {
                auto const naive = phase < FloatType(0.5)
                    ? FloatType(1)
                    : FloatType(-1);

                auto const halfPhase = std::fmod(phase + FloatType(0.5), FloatType(1));
                auto const square = naive + blep(phase, increment) - blep(halfPhase, increment);

                if constexpr (Mode == detail::BlepMode::Square) {
                    return square;
                }

                acc += FloatType(4) * increment * square;
                return acc;
            }

            return FloatType(0);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, bail here and zero the buffer
            if (numChannels < 1)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            FloatType const sr = elem::GraphNode<FloatType>::getSampleRate();

            for (size_t i = 0; i < numSamples; ++i) {
                auto freq = inputData[0][i];
                auto increment = freq / sr;

                auto out = tick(phase, increment);

                phase += increment;

                if (phase >= FloatType(1))
                    phase -= FloatType(1);

                outputData[i] = out;
            }
        }

        FloatType phase = 0;
        FloatType acc = 0;
    };

} // namespace elem

