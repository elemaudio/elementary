#pragma once

#include "../../GraphNode.h"
#include "../../SingleWriterSingleReaderQueue.h"
#include "../../Types.h"

#include "../helpers/Change.h"
#include "../helpers/GainFade.h"
#include "../helpers/FloatUtils.h"


namespace elem
{

    template <typename FloatType>
    struct MCVariablePitchReader;

    // SampleNode is a core builtin for sample playback.
    //
    // The sample file is loaded from disk or from virtual memory with a path set by the `path` property.
    // The sample is then triggered on the rising edge of an incoming pulse train, so
    // this node expects a single child node delivering that train.
    template <typename FloatType>
    struct MCSampleNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val, SharedResourceMap& resources) override
        {
            if (key == "path") {
                if (!val.isString())
                    return ReturnCode::InvalidPropertyType();

                if (!resources.has((js::String) val))
                    return ReturnCode::InvalidPropertyValue();

                auto ref = resources.get((js::String) val);
                bufferQueue.push(std::move(ref));
            }

            if (key == "mode") {
                if (!val.isString())
                    return ReturnCode::InvalidPropertyType();

                auto v = (js::String) val;

                if (v == "trigger") { mode.store(Mode::Trigger); }
                if (v == "gate") { mode.store(Mode::Gate); }
                if (v == "loop") { mode.store(Mode::Loop); }
            }

            if (key == "startOffset") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                auto const v = (js::Number) val;
                auto const vi = static_cast<int>(v);

                if (vi < 0)
                    return ReturnCode::InvalidPropertyValue();

                startOffset.store(static_cast<size_t>(vi));
            }

            if (key == "stopOffset") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                auto const v = (js::Number) val;
                auto const vi = static_cast<int>(v);

                if (vi < 0)
                    return ReturnCode::InvalidPropertyValue();

                stopOffset.store(static_cast<size_t>(vi));
            }

            if (key == "playbackRate") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                playbackRate.store((js::Number) val);
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void reset() override {
            readers[0].noteOff();
            readers[1].noteOff();
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto** outputData = ctx.outputData;
            auto numIns = ctx.numInputChannels;
            auto numOuts = ctx.numOutputChannels;
            auto numSamples = ctx.numSamples;

            auto const sampleRate = GraphNode<FloatType>::getSampleRate();

            // First order of business: grab the most recent sample buffer to use if
            // there's anything in the queue. This behavior means that changing the buffer
            // while playing the sample will cause a discontinuity.
            while (bufferQueue.size() > 0) {
                bufferQueue.pop(activeBuffer);

                readers[0] = MCVariablePitchReader<FloatType>(sampleRate, activeBuffer);
                readers[1] = MCVariablePitchReader<FloatType>(sampleRate, activeBuffer);
            }

            // First we clear the output buffers
            for (size_t j = 0; j < numOuts; ++j) {
                std::fill_n(outputData[j], numSamples, FloatType(0));
            }

            // Then if we don't have an input trigger or an active buffer,
            // we can then just return here
            if (numIns < 1 || activeBuffer == nullptr) {
                return;
            }

            // Now we expect the first input channel to carry a pulse train, and we
            // look through that input for the next rising edge. When that edge is found,
            // we process the current chunk and then allocate the next reader.
            auto const playbackMode = mode.load();
            auto const wantsLoop = mode == Mode::Loop;
            auto const ostart = startOffset.load();
            auto const ostop = stopOffset.load();
            auto const rate = playbackRate.load();

            size_t i = 0;
            size_t j = 0;

            for (j = 0; j < numSamples; ++j) {
                auto cv = change(inputData[0][j]);

                if (cv > FloatType(0.5)) {
                    // Read from [i, j]
                    readers[0].sumInto(outputData, numOuts, i, j - i, rate);
                    readers[1].sumInto(outputData, numOuts, i, j - i, rate);

                    // Update voice state
                    readers[currentReader & 1].noteOff();
                    readers[++currentReader & 1].noteOn(ostart, ostop, wantsLoop);

                    // Update counters
                    i = j;
                }

                // If we're in trigger mode then we can ignore falling edges
                if (cv < FloatType(-0.5) && playbackMode != Mode::Trigger) {
                    // Read from [i, j]
                    readers[0].sumInto(outputData, numOuts, i, j - i, rate);
                    readers[1].sumInto(outputData, numOuts, i, j - i, rate);

                    // Update voice state
                    readers[currentReader & 1].noteOff();

                    // Break so we can update our sample counters
                    // Update counters
                    i = j;
                }
            }

            readers[0].sumInto(outputData, numOuts, i, j - i, rate);
            readers[1].sumInto(outputData, numOuts, i, j - i, rate);
        }

        SingleWriterSingleReaderQueue<SharedResourcePtr> bufferQueue;
        SharedResourcePtr activeBuffer;

        Change<FloatType> change;
        std::array<MCVariablePitchReader<FloatType>, 2> readers;
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
        std::atomic<double> playbackRate = 1.0;
    };

    // A helper struct for reading from sample data with variable rate using
    // linear interpolation.
    template <typename FloatType>
    struct MCVariablePitchReader
    {
        MCVariablePitchReader()
            : sourceBuffer(nullptr), gainFade(44100.0, 4.0, 4.0)
        {}

        MCVariablePitchReader(FloatType _sampleRate, SharedResourcePtr _sourceBuffer)
            : sourceBuffer(_sourceBuffer), gainFade(_sampleRate, 4.0, 4.0)
        {}

        MCVariablePitchReader(MCVariablePitchReader& other)
            : sourceBuffer(other.sourceBuffer), gainFade(other.gainFade)
        {}

        void noteOn(size_t _startOffset, size_t _stopOffset, bool wantsLoop)
        {
            gainFade.fadeIn();

            startOffset = (double) _startOffset;
            stopOffset = (double) _stopOffset;
            shouldLoop = wantsLoop;
            pos = startOffset;
        }

        void noteOff()
        {
            gainFade.fadeOut();
        }

        FloatType lerpRead(BufferView<float> const& view, double pos)
        {
            auto* data = view.data();
            auto size = view.size();

            auto left = static_cast<size_t>(pos);
            auto right = left + 1;
            auto alpha = pos - (double) left;

            if (left >= size)
                return FloatType(0);

            if (right >= size)
                return data[left];

            return lerp(static_cast<float>(alpha), data[left], data[right]);
        }

        void sumInto(FloatType** outputData, size_t numOuts, size_t writeOffset, size_t numSamples, double playbackRate)
        {
            elem::GainFade<FloatType> localFade(gainFade);

            double readStart = startOffset;
            double readStop = 0;

            for (size_t i = 0; i < std::min(numOuts, sourceBuffer->numChannels()); ++i) {
                auto bufferView = sourceBuffer->getChannelData(i);
                size_t const sourceLength = bufferView.size();

                readStop = static_cast<double>(sourceLength) - stopOffset;

                // Reinitialize the local copy to match our member instance
                localFade = gainFade;

                for (size_t j = 0; j < numSamples; ++j) {
                    double readPos = pos + static_cast<double>(j) * playbackRate;

                    if (readPos >= readStop) {
                        if (shouldLoop) {
                            readPos = readStart + std::fmod(readPos - readStart, readStop - readStart);
                        } else {
                            continue;
                        }
                    }

                    outputData[i][writeOffset + j] += localFade(lerpRead(bufferView, readPos));
                }
            }

            // Here we have a localFade instance that has finished running over a block, which
            // represents where our class instance should now be
            gainFade = localFade;

            // And update our position
            pos += static_cast<double>(numSamples) * playbackRate;

            if (pos >= readStop && shouldLoop) {
                pos = readStart + std::fmod(pos - readStart, readStop - readStart);
            }
        }

        SharedResourcePtr sourceBuffer;

        GainFade<FloatType> gainFade;
        bool shouldLoop = false;
        double stopOffset = 0;
        double startOffset = 0;
        double pos = 0;
    };

} // namespace elem
