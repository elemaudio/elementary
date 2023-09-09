#pragma once

#include "../../GraphNode.h"


namespace elem
{

    // A linear State Variable Filter based on Andy Simper's work
    //
    // See: https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf
    //
    // This filter supports one "mode" property and expects three children: one
    // which defines the cutoff frequency, one which defines the filter Q, and finally
    // the input signal itself. Accepting these inputs as audio signals allows for
    // fast modulation at the processing expense of computing the coefficients on
    // every tick of the filter.
    template <typename FloatType>
    struct StateVariableFilterNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        enum class Mode {
            Low = 0,
            Band = 1,
            High = 2,
            Notch = 3,
            All = 4,
        };

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "mode") {
                if (!val.isString())
                    return ReturnCode::InvalidPropertyType();

                auto const m = (js::String) val;

                if (m == "lowpass")     { _mode.store(Mode::Low); }
                if (m == "bandpass")    { _mode.store(Mode::Band); }
                if (m == "highpass")    { _mode.store(Mode::High); }
                if (m == "notch")       { _mode.store(Mode::Notch); }
                if (m == "allpass")     { _mode.store(Mode::All); }
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        inline FloatType tick (Mode m, FloatType v0) {
            double v3 = v0 - _ic2eq;
            double v1 = _ic1eq * _a1 + v3 * _a2;
            double v2 = _ic2eq + _ic1eq * _a2 + v3 * _a3;

            _ic1eq = v1 * 2.0 - _ic1eq;
            _ic2eq = v2 * 2.0 - _ic2eq;

            switch (m) {
                case Mode::Low:
                    return FloatType(v2);
                case Mode::Band:
                    return FloatType(v1);
                case Mode::High:
                    return FloatType(v0 - _k * v1 - v2);
                case Mode::Notch:
                    return FloatType(v0 - _k * v1);
                case Mode::All:
                    return FloatType(v0 - 2.0 * _k * v1);
                default:
                    return FloatType(0);
            }
        }

        inline void updateCoeffs (double fc, double q) {
            auto const sr = GraphNode<FloatType>::getSampleRate();

            _g = std::tan(3.14159265359 * std::clamp(fc, 20.0, sr / 2.0001)  / sr);
            _k = 1.0 / std::clamp(q, 0.25, 20.0);
            _a1 = 1.0 / (1.0 + _g * (_g + _k));
            _a2 = _g * _a1;
            _a3 = _g * _a2;
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            auto m = _mode.load();

            // If we don't have the inputs we need, bail here and zero the buffer
            if (numChannels < 3)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            for (size_t i = 0; i < numSamples; ++i) {
                auto fc = inputData[0][i];
                auto q = inputData[1][i];
                auto xn = inputData[2][i];

                // Update coeffs at audio rate
                updateCoeffs(fc, q);

                // Tick the filter
                outputData[i] = tick(m, xn);
            }
        }

        // Props
        std::atomic<Mode> _mode { Mode::Low };
        static_assert(std::atomic<Mode>::is_always_lock_free);

        // Coefficients
        double _g = 0;
        double _k = 0;
        double _a1 = 0;
        double _a2 = 0;
        double _a3 = 0;

        // State
        double _ic1eq = 0;
        double _ic2eq = 0;
    };

} // namespace elem
