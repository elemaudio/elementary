#pragma once

#include "../helpers/FloatUtils.h"
#include "../helpers/GainFade.h"


namespace elem
{

    namespace detail
    {

        template <typename FloatType>
        struct MCBufferReader {
            MCBufferReader(double sampleRate, double fadeTime)
                : fade(sampleRate, fadeTime)
            {
            }

            void engage (double start, double currentTime, size_t _bufferSize) {
                startTime = start;
                bufferSize = _bufferSize;
                fade.setTargetGain(FloatType(1));

                position = static_cast<size_t>(((currentTime - startTime) / sampleDuration) * (double) (bufferSize - 1u));
                position = std::clamp<size_t>(position, 0, bufferSize);
            }

            void disengage() {
                fade.setTargetGain(FloatType(0));
            }

            // Does the incoming time match what this reader is expecting?
            //
            // If we're not engaged, we don't have any expectations so we just say sure.
            // If we are engaged, we try to map the incoming time onto a position in the
            // buffer and see if that's far off from where we currently are.
            bool isAlignedWithTime(double t) {
                if (!fade.on())
                    return true;

                size_t newPos = static_cast<size_t>(((t - startTime) / sampleDuration) * (double) (bufferSize - 1u));
                int delta = static_cast<int>(position) - static_cast<int>(newPos);
                bool aligned = std::abs(delta) < 16;

                return aligned;
            }

            template <typename DestType>
            void readAdding(SharedResource* resource, DestType** outputData, size_t numChannels, size_t numSamples) {
                elem::GainFade<FloatType> localFade(fade);

                for (size_t j = 0; j < std::min(numChannels, resource->numChannels()); ++j) {
                    auto bufferView = resource->getChannelData(j);
                    auto bufferSize = bufferView.size();
                    auto* sourceData = bufferView.data();

                    // Reinitialize the local copy to match our member instance
                    localFade = fade;

                    for (size_t i = 0; (i < numSamples) && ((position + i) < bufferSize); ++i) {
                        outputData[j][i] += static_cast<DestType>(localFade(sourceData[position + i]));
                    }
                }

                // Here we have a localFade instance that has finished running over a block, which
                // represents where our class instance should now be
                fade = localFade;

                // And update our position
                position += numSamples;
            }

            void reset (double sampleDur) {
                fade.reset();

                sampleDuration = sampleDur;
                startTime = 0.0;
            }

            elem::GainFade<FloatType> fade;
            size_t bufferSize = 0;
            size_t position = 0;

            double sampleDuration = 0;
            double startTime = 0;
        };
    }

    template <typename FloatType, bool WithStretch = false>
    struct StereoSampleSeqNode : public GraphNode<FloatType> {
        StereoSampleSeqNode(NodeId id, FloatType const sr, int const blockSize)
            : GraphNode<FloatType>::GraphNode(id, sr, blockSize)
            , readers({detail::MCBufferReader<float>(sr, 8.0), detail::MCBufferReader<float>(sr, 8.0)})
        {
            if constexpr (WithStretch) {
                stretch.presetDefault(2, sr);

                // Enough space to scale 1 block into 4 for two different channels
                scratchBuffer.resize(blockSize * 4 * 2);
            }
        }

        size_t getNumOutputChannels() override
        {
            return 2;
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
                if (detail::fpEqual(prevEvent->second, FloatType(1.0))) {
                    readers[activeReader].engage(prevEvent->first, t, activeBuffer->numSamples());
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
                readers[0].reset(sampleDur);
                readers[1].reset(sampleDur);
                rtSampleDuration = sampleDur;
            }

            // Pull newest buffer from queue
            while (bufferQueue.size() > 0) {
                bufferQueue.pop(activeBuffer);

                readers[0].reset(sampleDur);
                readers[1].reset(sampleDur);
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

            // TODO: if the input time has changed significantly, need to address the input latency of
            // the phase vocoder by resetting it and then pushing stretch.inputLatency * stretchFactor
            // samples ahead of `timeInSamples(t)`
            if (shouldUpdateBounds || !readers[activeReader].isAlignedWithTime(t)) {
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

                readers[0].readAdding(activeBuffer.get(), scratchPtrs, ctx.numOutputChannels, numSourceSamples);
                readers[1].readAdding(activeBuffer.get(), scratchPtrs, ctx.numOutputChannels, numSourceSamples);

                stretch.process(scratchPtrs, static_cast<int>(numSourceSamples), outputData, static_cast<int>(numSamples));
            } else {
                // Clear and read
                for (size_t i = 0; i < activeBuffer->numChannels(); ++i) {
                    std::fill_n(outputData[i], numSamples, FloatType(0));
                }

                readers[0].readAdding(activeBuffer.get(), outputData, ctx.numOutputChannels, numSamples);
                readers[1].readAdding(activeBuffer.get(), outputData, ctx.numOutputChannels, numSamples);
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

        std::array<detail::MCBufferReader<float>, 2> readers;
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
