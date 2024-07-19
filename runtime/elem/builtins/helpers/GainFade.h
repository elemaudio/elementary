#pragma once

#include "./FloatUtils.h"


namespace elem
{

    namespace detail {
        inline double millisecondsToStep(double sampleRate, double ms) {
            return ms > 1e-6 ? 1.0 / (sampleRate * ms / 1000.0) : 1.0;
        }
    }

    template <typename FloatType>
    struct GainFade
    {
        GainFade(double sampleRate, double fadeInTimeMs, double fadeOutTimeMs, FloatType current = 0.0, FloatType target = 0.0)
        : currentGain(current)
        , targetGain(target)
        {
            setFadeInTimeMs(sampleRate, fadeInTimeMs);
            setFadeOutTimeMs(sampleRate, fadeOutTimeMs);
        }

        GainFade(GainFade const& other)
        : currentGain(other.currentGain.load())
        , targetGain(other.targetGain.load())
        , inStep(other.inStep.load())
        , outStep(other.outStep.load())
        {
        }

        void operator= (GainFade const& other)
        {
            currentGain.store(other.currentGain.load());
            targetGain.store(other.targetGain.load());
            inStep.store(other.inStep.load());
            outStep.store(other.outStep.load());
        }

        FloatType operator() (FloatType x) {
            auto const _currentGain = currentGain.load();
            auto const _targetGain = targetGain.load();
            if (_currentGain == _targetGain)
                return (_currentGain * x);

            auto y = x * _currentGain;
            auto const step = _currentGain < _targetGain ? inStep.load() : outStep.load();
            currentGain.store(std::clamp(_currentGain + step, FloatType(0), FloatType(1)));

            return y;
        }

        void setFadeInTimeMs(double sampleRate, double fadeInTimeMs) {
            inStep.store(detail::millisecondsToStep(sampleRate, fadeInTimeMs));
        }

        void setFadeOutTimeMs(double sampleRate, double fadeOutTimeMs) {
            outStep.store(detail::millisecondsToStep(sampleRate, fadeOutTimeMs));
        }
        
        void setCurrentGain(FloatType gain) {
            currentGain.store(gain);
        }

        void setTargetGain(FloatType gain) {
            targetGain.store(gain);
        }

        bool on() {
            return (targetGain.load() > FloatType(0.5));
        }

        bool settled() {
            return fpEqual(targetGain.load(), currentGain.load());
        }

        void reset() {
            currentGain.store(FloatType(0));
            targetGain.store(FloatType(0));
        }

    private:
        std::atomic<FloatType> currentGain = 0;
        std::atomic<FloatType> targetGain = 0;
        std::atomic<FloatType> inStep = 0;
        std::atomic<FloatType> outStep = 0;
    };

} // namespace elem
