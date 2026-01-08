#pragma once

#include "../GraphNode.h"
#include "../SingleWriterSingleReaderQueue.h"
#include "../Types.h"

#include "./helpers/Change.h"
#include "elem/builtins/helpers/BufferReader.h"


namespace elem
{

    // SampleNode is a core builtin for sample playback.
    //
    // The sample file is loaded from disk or from virtual memory with a path set by the `path` property.
    // The sample is then triggered on the rising edge of an incoming pulse train, so
    // this node expects a single child node delivering that train.
    template <typename FloatType>
    struct SampleNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;
        using ReaderContext = typename BufferReader<FloatType>::template ReadContext<FloatType>;

        SampleNode(NodeId id, double sr, size_t blockSize)
            : GraphNode<FloatType>::GraphNode(id, sr, blockSize)
            , readers({BufferReader<FloatType>(sr, 8.0), BufferReader<FloatType>(sr, 8.0)})
        {
        }

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

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void reset() override {
            readers[0].disengage();
            readers[1].disengage();
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData[0];
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            auto const sampleRate = GraphNode<FloatType>::getSampleRate();

            // First order of business: grab the most recent sample buffer to use if
            // there's anything in the queue. This behavior means that changing the buffer
            // while playing the sample will cause a discontinuity.
            while (bufferQueue.size() > 0) {
                bufferQueue.pop(activeBuffer);
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
                    readers[currentReader & 1].disengage();
                    readers[++currentReader & 1].engage(0);
                }

                // If we're in trigger mode then we can ignore falling edges
                if (cv < FloatType(-0.5) && playbackMode != Mode::Trigger) {
                    readers[currentReader & 1].disengage();
                }

                auto outputChannels = std::array{&outputData[i]};
                // Process both readers for the current sample
                std::for_each(readers.begin(), readers.end(), [&](auto& reader) {
                    reader.readAdding(ReaderContext{
                        .source = activeBuffer.get(),
                        .outputData = outputChannels.data(),
                        .numChannels = 1,
                        .numSamples = 1,
                        .startOffsetSamples = ostart,
                        .stopOffsetSamples = ostop,
                        .shouldLoop = wantsLoop,
                        .playbackRate = rate,
                    });
                });
            }
        }

        SingleWriterSingleReaderQueue<SharedResourcePtr> bufferQueue;
        SharedResourcePtr activeBuffer;

        Change<FloatType> change;
        std::array<BufferReader<FloatType>, 2> readers;
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

} // namespace elem
