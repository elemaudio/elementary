#pragma once

#include "../GraphNode.h"
#include "../Invariant.h"
#include "../SingleWriterSingleReaderQueue.h"
#include "../Types.h"

#include "./helpers/Change.h"


namespace elem
{

    template <typename FloatType>
    struct VariablePitchLerpReader;

    // SampleNode is a core builtin for sample playback.
    //
    // The sample file is loaded from disk or from virtual memory with a path set by the `path` property.
    // The sample is then triggered on the rising edge of an incoming pulse train, so
    // this node expects a single child node delivering that train.
    template <typename FloatType, typename ReaderType = VariablePitchLerpReader<FloatType>>
    struct SampleNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void setProperty(std::string const& key, js::Value const& val, SharedResourceMap<FloatType>& resources) override
        {
            GraphNode<FloatType>::setProperty(key, val);

            if (key == "path") {
                invariant(val.isString(), "path prop must be a string");
                invariant(resources.has((js::String) val), "failed to find a resource at the given path");

                auto ref = resources.get((js::String) val);
                bufferQueue.push(std::move(ref));
            }

            if (key == "mode") {
                invariant(val.isString(), "mode prop for the sample node must be a string.");
                auto v = (js::String) val;

                if (v == "trigger") { mode.store(Mode::Trigger); }
                if (v == "gate") { mode.store(Mode::Gate); }
                if (v == "loop") { mode.store(Mode::Loop); }
            }

            if (key == "startOffset") {
                invariant(val.isNumber(), "startOffset prop for the sample node must be a number.");

                auto const v = (js::Number) val;
                auto const vi = static_cast<int>(v);

                invariant(vi >= 0, "startOffset prop for the sample node must be a positive number.");
                startOffset.store(static_cast<size_t>(vi));
            }

            if (key == "stopOffset") {
                invariant(val.isNumber(), "stopOffset prop for the sample node must be a number.");

                auto const v = (js::Number) val;
                auto const vi = static_cast<int>(v);

                invariant(vi >= 0, "stopOffset prop for the sample node must be a positive number.");
                stopOffset.store(static_cast<size_t>(vi));
            }
        }

        void reset() override {
            readers[0].noteOff();
            readers[1].noteOff();
        }

        void process (const FloatType** inputData, FloatType* outputData, size_t const numChannels, size_t const numSamples, int64_t) override {
            auto const sampleRate = GraphNode<FloatType>::getSampleRate();

            // First order of business: grab the most recent sample buffer to use if
            // there's anything in the queue. This behavior means that changing the buffer
            // while playing the sample will cause a discontinuity.
            while (bufferQueue.size() > 0) {
                bufferQueue.pop(activeBuffer);

                readers[0] = ReaderType(sampleRate, activeBuffer);
                readers[1] = ReaderType(sampleRate, activeBuffer);
            }

            // If we don't have an input trigger or an active buffer, we can just return here
            if (numChannels < 1 || activeBuffer == nullptr)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // Now we expect the first input channel to carry a pulse train, and we
            // look through that input for the next rising edge. When that edge is found,
            // we process the current chunk and then allocate the next reader.
            auto const playbackMode = mode.load();
            auto const wantsLoop = mode == Mode::Loop;
            auto const ostart = startOffset.load();
            auto const ostop = stopOffset.load();

            // Optionally accept a second input signal specifying the playback rate
            auto const hasPlaybackRateSignal = numChannels >= 2;

            for (size_t i = 0; i < numSamples; ++i) {
                auto cv = change(inputData[0][i]);
                auto const rate = hasPlaybackRateSignal ? inputData[1][i] : FloatType(1);

                // Rising edge
                if (cv > FloatType(0.5)) {
                    readers[currentReader & 1].noteOff();
                    readers[++currentReader & 1].noteOn(ostart);
                }

                // If we're in trigger mode then we can ignore falling edges
                if (cv < FloatType(-0.5) && playbackMode != Mode::Trigger) {
                    readers[currentReader & 1].noteOff();
                }

                // Process both readers for the current sample
                outputData[i] = readers[0].tick(ostart, ostop, rate, wantsLoop) + readers[1].tick(ostart, ostop, rate, wantsLoop);
            }
        }

        SingleWriterSingleReaderQueue<SharedResourceBuffer<FloatType>> bufferQueue;
        SharedResourceBuffer<FloatType> activeBuffer;

        Change<FloatType> change;
        std::array<ReaderType, 2> readers;
        size_t currentReader = 0;

        enum class Mode
        {
            Trigger = 0,
            Gate = 1,
            Loop = 2,
        };

        std::atomic<Mode> mode = Mode::Trigger;
        std::atomic<size_t> startOffset = 0;
        std::atomic<size_t> stopOffset = 0;
    };

    // A helper struct for reading from sample data with variable rate using
    // linear interpolation.
    template <typename FloatType>
    struct VariablePitchLerpReader
    {
        VariablePitchLerpReader() = default;

        VariablePitchLerpReader(FloatType _sampleRate, SharedResourceBuffer<FloatType> _sourceBuffer)
            : sourceBuffer(_sourceBuffer), sampleRate(_sampleRate), gainSmoothAlpha(1.0 - std::exp(-1.0 / (0.01 * _sampleRate))) {}

        VariablePitchLerpReader(VariablePitchLerpReader& other)
            : sourceBuffer(other.sourceBuffer), sampleRate(other.sampleRate), gainSmoothAlpha(1.0 - std::exp(-1.0 / (0.01 * other.sampleRate))) {}

        void noteOn(size_t const startOffset)
        {
            targetGain = FloatType(1);
            pos = FloatType(startOffset);
        }

        void noteOff()
        {
            targetGain = FloatType(0);
        }

        FloatType tick (size_t const startOffset, size_t const stopOffset, FloatType const stepSize, bool const wantsLoop)
        {
            if (sourceBuffer == nullptr || pos < 0.0 || (gain == FloatType(0) && targetGain == FloatType(0)))
                return FloatType(0);

            auto* sourceData = sourceBuffer->data();
            size_t const sourceLength = sourceBuffer->size();

            if (pos >= (double) (sourceLength - stopOffset)) {
                if (!wantsLoop) {
                    return FloatType(0);
                }

                pos = (double) startOffset;
            }

            // Linear interpolation on the buffer read
            auto readLeft = static_cast<int>(pos);
            auto readRight = readLeft + 1;
            auto const frac = FloatType(pos - (double) readLeft);

            if (readLeft >= sourceLength)
                readLeft -= sourceLength;

            if (readRight >= sourceLength)
                readRight -= sourceLength;

            auto const left = sourceData[readLeft];
            auto const right = sourceData[readRight];

            // Now we can read the next sample out of the buffer with linear
            // interpolation for sub-sample reads.
            auto const out = gain * (left + frac * (right - left));
            auto const gainSettled = std::abs(targetGain - gain) <= std::numeric_limits<FloatType>::epsilon();

            // Update our state
            pos = pos + (double) stepSize;
            gain = gainSettled ? targetGain : gain + gainSmoothAlpha * (targetGain - gain);
            gain = std::clamp(gain, FloatType(0), FloatType(1));

            // And return
            return out;
        }

        SharedResourceBuffer<FloatType> sourceBuffer;

        FloatType sampleRate = 0;
        FloatType gainSmoothAlpha = 0;
        FloatType targetGain = 0;
        FloatType gain = 0;
        double pos = 0;
    };

} // namespace elem
