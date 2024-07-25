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
        , step(other.step.load())
        , inStep(other.inStep)
        , outStep(other.outStep)
        {
        }

        void operator= (GainFade const& other)
        {
            currentGain.store(other.currentGain.load());
            targetGain.store(other.targetGain.load());
            step.store(other.step.load());
            inStep = other.inStep;
            outStep = other.outStep;
        }

        FloatType operator() (FloatType x) {
            auto const _currentGain = currentGain.load();
            auto const _targetGain = targetGain.load();
            if (fpEqual(_currentGain, _targetGain))
                return (_targetGain * x);

            auto y = x * _currentGain;
            currentGain.store(std::clamp(_currentGain + step.load(), FloatType(0), FloatType(1)));

            return y;
        }

        void process (const FloatType* input, FloatType* output, int numSamples) {
            auto const _currentGain = currentGain.load();
            auto const _targetGain = targetGain.load();
            if (_currentGain == _targetGain) {
                for (int i = 0; i < numSamples; ++i) {
                    output[i] = input[i] * _targetGain;
                }
                return;
            }

            auto const _step = step.load();
            for (int i = 0; i < numSamples; ++i) {
                output[i] = input[i] * std::clamp(_currentGain + _step * i, FloatType(0), FloatType(1));
            }
            
            currentGain.store(std::clamp(_currentGain + _step * numSamples, FloatType(0), FloatType(1)));
        }

        void setFadeInTimeMs(double sampleRate, double fadeInTimeMs) {
            inStep = detail::millisecondsToStep(sampleRate, fadeInTimeMs);
            updateCurrentStep();
        }

        void setFadeOutTimeMs(double sampleRate, double fadeOutTimeMs) {
            outStep = FloatType(-1) * detail::millisecondsToStep(sampleRate, fadeOutTimeMs);
            updateCurrentStep();
        }

        void fadeIn() {
            targetGain.store(FloatType(1));
            updateCurrentStep();
        }

        void fadeOut() {
            targetGain.store(FloatType(0));
            updateCurrentStep();
        }

        void setCurrentGain(FloatType gain) {
            currentGain.store(gain);
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
        void updateCurrentStep() {
            step.store(currentGain.load() > targetGain.load() ? outStep : inStep);
        }

        std::atomic<FloatType> currentGain = 0;
        std::atomic<FloatType> targetGain = 0;
        std::atomic<FloatType> step = 0;
        FloatType inStep = 0;
        FloatType outStep = 0;
    };

} // namespace elem
