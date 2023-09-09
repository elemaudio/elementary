#pragma once

#include "../../GraphNode.h"


namespace elem
{

    // A linear State Variable Shelf Filter based on Andy Simper's work
    //
    // See: https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf
    //
    // This filter supports one "mode" property and expects four children: one
    // which defines the cutoff frequency, one which defines the filter Q, one which
    // defines the band gain (in decibels), and finally the input signal itself.
    //
    // Accepting these inputs as audio signals allows for fast modulation at the
    // processing expense of computing the coefficients on every tick of the filter.
    template <typename FloatType>
    struct StateVariableShelfFilterNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        enum class Mode {
            Lowshelf = 0,
            Highshelf = 1,
            Bell = 2,
        };

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "mode") {
                if (!val.isString())
                    return ReturnCode::InvalidPropertyType();

                auto const m = (js::String) val;

                if (m == "lowshelf")     { _mode.store(Mode::Lowshelf); }
                if (m == "highshelf")    { _mode.store(Mode::Highshelf); }

                if (m == "bell" || m == "peak") { _mode.store(Mode::Bell); }
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
                case Mode::Bell:
                    return FloatType(v0 + _k * (_A * _A - 1.0) * v1);
                case Mode::Lowshelf:
                    return FloatType(v0 + _k * (_A - 1.0) * v1 + (_A * _A - 1.0) * v2);
                case Mode::Highshelf:
                    return FloatType(_A * _A * v0 + _k * (1.0 - _A) * _A * v1 + (1.0 - _A * _A) * v2);
                default:
                    return FloatType(0);
            }
        }

        inline void updateCoeffs (Mode m, double fc, double q, double gainDecibels) {
            auto const sr = GraphNode<FloatType>::getSampleRate();

            _A = std::pow(10, gainDecibels / 40.0);
            _g = std::tan(3.14159265359 * std::clamp(fc, 20.0, sr / 2.0001)  / sr);
            _k = 1.0 / std::clamp(q, 0.25, 20.0);

            if (m == Mode::Lowshelf)
                _g /= _A;
            if (m == Mode::Highshelf)
                _g *= _A;
            if (m == Mode::Bell)
                _k /= _A;

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
            if (numChannels < 4)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            for (size_t i = 0; i < numSamples; ++i) {
                auto fc = inputData[0][i];
                auto q = inputData[1][i];
                auto gain = inputData[2][i];
                auto xn = inputData[3][i];

                // Update coeffs at audio rate
                updateCoeffs(m, fc, q, gain);

                // Tick the filter
                outputData[i] = tick(m, xn);
            }
        }

        // Props
        std::atomic<Mode> _mode { Mode::Lowshelf };
        static_assert(std::atomic<Mode>::is_always_lock_free);

        // Coefficients
        double _A = 0;
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
