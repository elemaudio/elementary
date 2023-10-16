#pragma once

#include "../../GraphNode.h"


namespace elem
{

    template <typename FloatType>
    struct CutoffPrewarpNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, bail here and zero the buffer
            if (numChannels < 1)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            double const T = 1.0 / GraphNode<FloatType>::getSampleRate();

            for (size_t i = 0; i < numSamples; ++i) {
                auto fc = inputData[0][i];

                // Cutoff prewarping
                double const twoPi = 2.0 * 3.141592653589793238;
                double const wd = twoPi * static_cast<double>(fc);
                double const g = std::tan(wd * T / 2.0);

                outputData[i] = FloatType(g);
            }
        }
    };

    template <typename FloatType>
    struct MultiMode1p : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        enum class Mode {
            Low = 0,
            High = 2,
            All = 4,
        };

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "mode") {
                if (!val.isString())
                    return ReturnCode::InvalidPropertyType();

                auto const m = (js::String) val;

                if (m == "lowpass")     { _mode.store(Mode::Low); }
                if (m == "highpass")    { _mode.store(Mode::High); }
                if (m == "allpass")     { _mode.store(Mode::All); }
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, bail here and zero the buffer
            if (numChannels < 2)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // Set up our output derivation
            auto const m = _mode.load();

            // Run the filter
            for (size_t i = 0; i < numSamples; ++i) {
                auto const g = std::clamp(static_cast<double>(inputData[0][i]), 0.0, 0.9999);
                auto xn = inputData[1][i];

                // Resolve the instantaneous gain
                double const G = g / (1.0 + g);

                // Tick the filter
                double const v = (static_cast<double>(xn) - z) * G;
                double const lp = v + z;

                z = lp + v;

                switch (m) {
                    case Mode::Low:
                        outputData[i] = FloatType(lp);
                        break;
                    case Mode::High:
                        outputData[i] = xn - FloatType(lp);
                        break;
                    case Mode::All:
                        outputData[i] = FloatType(lp + lp - xn);
                        break;
                    default:
                        break;
                }
            }
        }

        // Props
        std::atomic<Mode> _mode { Mode::Low };
        static_assert(std::atomic<Mode>::is_always_lock_free);

        // Coefficients
        double z = 0;
    };

} // namespace elem
