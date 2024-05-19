#pragma once

#include "./FloatUtils.h"


namespace elem
{

    template <typename FloatType>
    struct GainFade
    {
        GainFade(double sampleRate, double fadeTimeMs)
            : step(FloatType(1.0 / (sampleRate * fadeTimeMs / 1000.0)))
        {
        }

        GainFade(GainFade const& other)
            : currentGain(other.currentGain), targetGain(other.targetGain), step(other.step)
        {
        }

        void operator= (GainFade const& other)
        {
            currentGain = other.currentGain;
            targetGain = other.targetGain;
            step = other.step;
        }

        FloatType operator() (FloatType x) {
            if (currentGain == targetGain)
                return (currentGain * x);

            auto y = x * currentGain;
            currentGain = std::clamp(currentGain + step, FloatType(0), FloatType(1));

            return y;
        }

        void setTargetGain (FloatType g) {
            targetGain = g;

            if (targetGain < currentGain) {
                step = FloatType(-1) * std::abs(step);
            } else {
                step = std::abs(step);
            }
        }

        bool on() {
            return (targetGain > FloatType(0.5));
        }

        bool settled() {
            return fpEqual(targetGain, currentGain);
        }

        void reset() {
            currentGain = FloatType(0);
            targetGain = FloatType(0);
        }

        FloatType currentGain = 0;
        FloatType targetGain = 0;
        FloatType step = 0;
    };

} // namespace elem
