#pragma once

#include "elem/GraphNode.h"
#include "elem/SingleWriterSingleReaderQueue.h"
#include "elem/Types.h"
#include "elem/builtins/helpers/BufferReader.h"
#include "elem/builtins/helpers/RefCountedPool.h"
#include "elem/third-party/signalsmith-stretch/signalsmith-stretch.h"

namespace elem
{

    template <typename FloatType, bool WithStretch = false>
    struct StereoSampleSeqNode : public GraphNode<FloatType> {
        using ReaderContext = typename BufferReader<FloatType>::template ReadContext<FloatType>;

        StereoSampleSeqNode(NodeId id, FloatType const sr, int const blockSize)
            : GraphNode<FloatType>::GraphNode(id, sr, blockSize)
            , readers({BufferReader<FloatType>(sr, 8.0), BufferReader<FloatType>(sr, 8.0)})
        {
            if constexpr (WithStretch) {
                stretch.presetDefault(2, sr);

                // Enough space to scale 1 block into 4 for two different channels
                scratchBuffer.resize(blockSize * 4 * 2);
            }
        }

        int setProperty(std::string const& key, js::Value const& val, SharedResourceMap& resources) override
        {
            if constexpr (WithStretch) {
                if (key == "shift") {
                    if (!val.isNumber())
                        return ReturnCode::InvalidPropertyType();

                    auto shift = (js::Number) val;
                    stretch.setTransposeSemitones(shift);
                }

                if (key == "stretch") {
                    if (!val.isNumber())
                        return ReturnCode::InvalidPropertyType();

                    auto _stretchFactor = (js::Number) val;

                    if (_stretchFactor < 0.25 || _stretchFactor > 4.0)
                        return ReturnCode::InvalidPropertyValue();

                    stretchFactor.store(_stretchFactor);
                }
            }

            if (key == "duration") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                auto dur = (js::Number) val;

                if (dur <= 0.0)
                    return ReturnCode::InvalidPropertyValue();

                sampleDuration.store(dur);
            }

            if (key == "path") {
                if (!val.isString())
                    return ReturnCode::InvalidPropertyType();

                if (!resources.has((js::String) val))
                    return ReturnCode::InvalidPropertyValue();

                auto ref = resources.get((js::String) val);
                bufferQueue.push(std::move(ref));
            }

            if (key == "seq") {
                if (!val.isArray())
                    return ReturnCode::InvalidPropertyType();

                auto& seq = val.getArray();

                if (seq.size() == 0)
                    return ReturnCode::InvalidPropertyValue();

                auto data = seqPool.allocate();

                // The data array that we get from the pool may have been
                // previously used to represent a different sequence
                data->clear();

                // We expect from the JavaScript side an array of event objects, where each
                // event includes a value to take and a time at which to take that value
                for (size_t i = 0; i < seq.size(); ++i) {
                    auto& event = seq[i].getObject();

                    FloatType value = static_cast<FloatType>((js::Number) event.at("value"));
                    double time = static_cast<double>((js::Number) event.at("time"));

                    data->insert({ time, value });
                }

                seqQueue.push(std::move(data));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void updateEventBoundaries(double t) {
            nextEvent = activeSeq->upper_bound(t);

            // The next event is the first one in the sequence
            if (nextEvent == activeSeq->begin()) {
                prevEvent = activeSeq->end();

                // Here we know that nothing should be playing, so, easy:
                readers[0].disengage();
                readers[1].disengage();
            } else {
                prevEvent = std::prev(nextEvent);

                // And here we decide based on the previous event
                readers[activeReader].disengage();
                activeReader = (activeReader + 1) & (readers.size() - 1);

                // Here a value of 1.0 is considered an onset, and anything else
                // considered an offset.
                if (fpEqual(prevEvent->second, FloatType(1.0))) {
                    // Map t to a normalized position relative to the sample
                    double const pos = rtSampleDuration > 0.0 ? prevEvent->first / rtSampleDuration : 0.0;
                    readers[activeReader].engage(pos);
                }
            }
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto** outputData = ctx.outputData;
            auto numSamples = ctx.numSamples;

            // Load sample duration
            auto const sampleDur = sampleDuration.load();

            if (sampleDur != rtSampleDuration) {
                readers[0].reset();
                readers[1].reset();
                rtSampleDuration = sampleDur;
            }

            // Pull newest buffer from queue
            while (bufferQueue.size() > 0) {
                bufferQueue.pop(activeBuffer);

                readers[0].reset();
                readers[1].reset();
            }

            // Pull newest seq from queue
            if (seqQueue.size() > 0) {
                while (seqQueue.size() > 0) {
                    seqQueue.pop(activeSeq);
                }

                // New sequence means we'll have to find our new event boundaries given
                // the current input time
                prevEvent = activeSeq->end();
                nextEvent = activeSeq->end();
            }

            // Next, if we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (ctx.numInputChannels < 1 || activeSeq == nullptr || activeBuffer == nullptr || sampleDur <= 0.0) {
                for (size_t i = 0; i < ctx.numOutputChannels; ++i) {
                    std::fill_n(outputData[i], numSamples, FloatType(0));
                }

                return;
            }

            // We reference this a lot
            auto const seqEnd = activeSeq->end();
            auto* scratchData = scratchBuffer.data();

            // Helpers to add some tolerance to the time checks
            auto const before = [](double t1, double t2) { return t1 <= (t2 + 1e-6); };
            auto const after = [](double t1, double t2) { return t1 >= (t2 - 1e-6); };

            // Downsampling from a-rate to k-rate
            auto const t = static_cast<double>(inputData[0][0]);

            // We update our event boundaries if we just took a new sequence, if we've stepped
            // forwards or backwards over the next event time, or if the incoming time step differs
            // excessively from what we expected
            auto const shouldUpdateBounds = (prevEvent == seqEnd && nextEvent == seqEnd)
                || (prevEvent != seqEnd && before(t, prevEvent->first))
                || (nextEvent != seqEnd && after(t, nextEvent->first));

            double const timeUnitsPerSample = sampleDur / (double) activeBuffer->numSamples();
            int64_t const sampleTime = t / timeUnitsPerSample;
            bool const significantTimeChange = std::abs(sampleTime - nextExpectedBlockStart) > 16;
            nextExpectedBlockStart = sampleTime + numSamples;

            // TODO: if the input time has changed significantly, need to address the input latency of
            // the phase vocoder by resetting it and then pushing stretch.inputLatency * stretchFactor
            // samples ahead of `timeInSamples(t)`
            if (shouldUpdateBounds || significantTimeChange) {
                updateEventBoundaries(t);
            }

            if constexpr (WithStretch) {
                // Some fractional sample counting here. Every time we calculate the number of
                // source samples, we inevitably leave a little rounding error. To ensure we
                // average out correctly over time, we accumulate that rounding error and nudge
                // our numSourceSamples once the accumulated error exceeds a full sample.
                double const trueSourceSamples = (double) numSamples / stretchFactor.load();
                size_t numSourceSamples = static_cast<size_t>(trueSourceSamples);

                accFracSamples += (trueSourceSamples - (double) numSourceSamples);

                if (accFracSamples >= 1.0) {
                    accFracSamples -= 1.0;
                    numSourceSamples++;
                }

                numSourceSamples = std::clamp(numSourceSamples, static_cast<size_t>(0), scratchBuffer.size());

                // Clear and read
                std::fill_n(scratchData, scratchBuffer.size(), FloatType(0));

                // TODO: Hax obviously. Should make an AudioBuffer class which uses a contiguous std vector for
                // storage and a SmallVector for the pointers pointing into the data. Choc?
                std::array<FloatType*, 2> ptrs {{scratchData, scratchData + (numSamples * 4)}};
                auto** scratchPtrs = ptrs.data();

                readers[0].readAdding(ReaderContext{
                    .source = activeBuffer.get(),
                    .outputData = scratchPtrs,
                    .numChannels = ctx.numOutputChannels,
                    .numSamples = numSourceSamples,
                });
                readers[1].readAdding(ReaderContext{
                    .source = activeBuffer.get(),
                    .outputData = scratchPtrs,
                    .numChannels = ctx.numOutputChannels,
                    .numSamples = numSourceSamples,
                });

                stretch.process(scratchPtrs, static_cast<int>(numSourceSamples), outputData, static_cast<int>(numSamples));
            } else {
                // Clear and read
                for (size_t i = 0; i < activeBuffer->numChannels(); ++i) {
                    std::fill_n(outputData[i], numSamples, FloatType(0));
                }

                readers[0].readAdding(ReaderContext{
                    .source = activeBuffer.get(),
                    .outputData = outputData,
                    .numChannels = ctx.numOutputChannels,
                    .numSamples = numSamples,
                });
                readers[1].readAdding(ReaderContext{
                    .source = activeBuffer.get(),
                    .outputData = outputData,
                    .numChannels = ctx.numOutputChannels,
                    .numSamples = numSamples,
                });
            }
        }

        using Sequence = std::map<double, FloatType, std::less<double>>;

        RefCountedPool<Sequence> seqPool;
        SingleWriterSingleReaderQueue<std::shared_ptr<Sequence>> seqQueue;
        std::shared_ptr<Sequence> activeSeq;

        typename Sequence::iterator prevEvent;
        typename Sequence::iterator nextEvent;

        SingleWriterSingleReaderQueue<SharedResourcePtr> bufferQueue;
        SharedResourcePtr activeBuffer;

        std::array<BufferReader<FloatType>, 2> readers;
        size_t activeReader = 0;
        int64_t nextExpectedBlockStart = 0;

        std::atomic<double> sampleDuration = 0;
        double rtSampleDuration = 0;

        signalsmith::stretch::SignalsmithStretch<FloatType> stretch;
        double accFracSamples = 0;
        std::atomic<double> stretchFactor = 1.0;
        std::vector<FloatType> scratchBuffer;
    };

    template <typename FloatType>
    using StereoSampleSeqWithStretchNode = StereoSampleSeqNode<FloatType, true>;

} // namespace elem
