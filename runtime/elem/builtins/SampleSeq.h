#pragma once

#include <map>
#include <iostream>

#include "../GraphNode.h"
#include "../SingleWriterSingleReaderQueue.h"
#include "../Types.h"
#include "helpers/BufferReader.h"
#include "helpers/RefCountedPool.h"

#include "../third-party/signalsmith-stretch/signalsmith-stretch.h"

namespace elem
{

    template <typename FloatType, bool WithStretch = false>
    struct SampleSeqNode : public GraphNode<FloatType> {
        using ReaderContext = typename BufferReader<FloatType>::template ReadContext<FloatType>;

        // Note: this is set to 1.1 ms to preserve backwards compatibility. Historically, a gain fade with
        // a step size of 0.02 was used for SampleSeqNode, which comes out to roughly 1.1 ms at 44100 Hz.
        static constexpr double FadeTime = 1.1;

        SampleSeqNode(NodeId id, FloatType const sr, int const blockSize)
            : GraphNode<FloatType>::GraphNode(id, sr, blockSize)
            , readers({BufferReader<FloatType>(sr, FadeTime), BufferReader<FloatType>(sr, FadeTime)})
        {
            if constexpr (WithStretch) {
                stretch.presetDefault(1, sr);
                scratchBuffer.resize(blockSize * 4);
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
                    readers[activeReader].engage(0);
                }
            }
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData[0];
            auto numChannels = ctx.numInputChannels;
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
            if (numChannels < 1 || activeSeq == nullptr || activeSeq->size() == 0 || activeBuffer == nullptr || sampleDur <= 0.0)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

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
                std::fill_n(scratchData, numSourceSamples, FloatType(0));

                std::for_each(readers.begin(), readers.end(), [&](auto& reader) {
                    reader.readAdding(ReaderContext(
                        activeBuffer.get(),
                        &scratchData,
                        1,
                        numSourceSamples
                    ));
                });

                stretch.process(&scratchData, numSourceSamples, &outputData, numSamples);
            } else {
                // Clear and read
                std::fill_n(outputData, numSamples, FloatType(0));

                std::for_each(readers.begin(), readers.end(), [&](auto& reader) {
                    reader.readAdding(ReaderContext(
                        activeBuffer.get(),
                        &outputData,
                        1,
                        numSamples
                    ));
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
    using SampleSeqWithStretchNode = SampleSeqNode<FloatType, true>;

} // namespace elem
