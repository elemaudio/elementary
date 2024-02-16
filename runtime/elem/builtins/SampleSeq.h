#pragma once

#include "../GraphNode.h"
#include "../SingleWriterSingleReaderQueue.h"
#include "../Types.h"

#include "helpers/RefCountedPool.h"
#include "../third-party/signalsmith-stretch/signalsmith-stretch.h"

#include <map>
#include <iostream>


namespace elem
{

    namespace detail
    {
        template <typename FloatType>
        FloatType lerp (FloatType alpha, FloatType x, FloatType y) {
            return x + alpha * (y - x);
        }

        template <typename FloatType>
        FloatType fpEqual (FloatType x, FloatType y) {
            return std::abs(x - y) <= FloatType(1e-6);
        }

        template <typename FloatType>
        struct GainFade {
            GainFade() = default;

            void setTargetGain (FloatType g) {
                targetGain = g;

                if (targetGain < currentGain) {
                    step = FloatType(-1) * std::abs(step);
                } else {
                    step = std::abs(step);
                }
            }

            FloatType operator() (FloatType x) {
                if (currentGain == targetGain)
                    return (currentGain * x);

                auto y = x * currentGain;
                currentGain = std::clamp(currentGain + step, FloatType(0), FloatType(1));

                return y;
            }

            bool on() {
                return fpEqual(targetGain, FloatType(1));
            }

            bool silent() {
                return fpEqual(targetGain, FloatType(0)) && fpEqual(currentGain, FloatType(0));
            }

            void reset() {
                currentGain = FloatType(0);
                targetGain = FloatType(0);
            }

            FloatType currentGain = 0;
            FloatType targetGain = 0;
            FloatType step = 0.02; // TODO
        };

        template <typename FloatType>
        struct BufferReader {
            BufferReader() = default;

            void engage (double start, double currentTime, FloatType* _buffer, size_t _size) {
                startTime = start;
                buffer = _buffer;
                bufferSize = _size;
                fade.setTargetGain(FloatType(1));

                position = static_cast<size_t>(((currentTime - startTime) / sampleDuration) * (double) (bufferSize - 1u));
                position = std::clamp<size_t>(position, 0, bufferSize);
            }

            void disengage() {
                fade.setTargetGain(FloatType(0));
            }

            void readAdding(FloatType* outputData, size_t numSamples) {
                for (size_t i = 0; (i < numSamples) && (position < bufferSize); ++i) {
                    outputData[i] += fade(buffer[position++]);
                }
            }

            FloatType read (FloatType const* buffer, size_t size, double t)
            {
                if (fade.silent() || sampleDuration <= FloatType(0))
                    return FloatType(0);

                // An allocated but inactive reader is currently fading out at the point in time
                // from which we jumped to allocate a new reader
                double const pos = fade.on()
                    ? (t - startTime) / sampleDuration
                    : (stepStopTime() - startTime) / sampleDuration;

                // While we're still active, track last position so that we can stop effectively
                if (fade.on()) {
                    dt = t - lastTimeStep;
                    lastTimeStep = t;
                }

                // Deallocate if we've run out of bounds
                if (pos < 0.0 || pos >= 1.0) {
                    disengage();
                    return FloatType(0);
                }

                // Instead of clamping here, we could accept loop points in the sample and
                // mod the playback position within those loop points. Property loop: [start, stop]
                auto l = static_cast<size_t>(pos * (double) (size - 1u));
                auto r = std::min(size, l + 1u);
                auto const alpha = FloatType((pos * (double) (size - 1u)) - static_cast<double>(l));

                return fade(lerp(alpha, buffer[l], buffer[r]));
            }

            FloatType stepStopTime() {
                lastTimeStep += dt;
                return lastTimeStep;
            }

            void reset (double sampleDur) {
                fade.reset();

                sampleDuration = sampleDur;
                startTime = 0.0;
                dt = 0.0;
            }

            GainFade<FloatType> fade;
            FloatType* buffer = nullptr;
            size_t bufferSize = 0;
            size_t position = 0;

            double sampleDuration = 0;
            double startTime = 0;
            double lastTimeStep = 0;
            double dt = 0;
        };
    }

    template <typename FloatType, bool WithStretch = false>
    struct SampleSeqNode : public GraphNode<FloatType> {
        SampleSeqNode(NodeId id, FloatType const sr, int const blockSize)
            : GraphNode<FloatType>::GraphNode(id, sr, blockSize)
        {
            if constexpr (WithStretch) {
                stretch.presetDefault(1, sr);
                scratchBuffer.resize(blockSize * 4);
            }
        }


        int setProperty(std::string const& key, js::Value const& val, SharedResourceMap<FloatType>& resources) override
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
                    readers[activeReader].engage(prevEvent->first, t, const_cast<FloatType*>(activeBuffer->data()), activeBuffer->size());
                }
            }
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
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
            if (numChannels < 1 || activeSeq == nullptr || activeBuffer == nullptr || sampleDur <= 0.0)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // We reference this a lot
            auto const seqEnd = activeSeq->end();
            auto* scratchData = scratchBuffer.data();

            // Helpers to add some tolerance to the time checks
            auto const before = [](double t1, double t2) { return t1 <= (t2 + 1e-6); };
            auto const after = [](double t1, double t2) { return t1 >= (t2 - 1e-6); };

            // Downsampling from a-rate to k-rate
            auto const t = static_cast<double>(inputData[0][0]);

            // Time check
            double const timeUnitsPerSample = sampleDur / (double) activeBuffer->size();
            int64_t const sampleTime = t / timeUnitsPerSample;
            bool const significantTimeChange = std::abs(sampleTime - nextExpectedBlockStart) > 16;
            nextExpectedBlockStart = sampleTime + numSamples;

            // We update our event boundaries if we just took a new sequence, if we've stepped
            // forwards or backwards over the next event time, or if the incoming time step differs
            // excessively from what we expected
            auto const shouldUpdateBounds = (prevEvent == seqEnd && nextEvent == seqEnd)
                || (prevEvent != seqEnd && before(t, prevEvent->first))
                || (nextEvent != seqEnd && after(t, nextEvent->first));

            // TODO: if the input time has changed significantly, need to address the input latency of
            // the phase vocoder by resetting it and then pushing stretch.inputLatency * stretchFactor
            // samples ahead of `timeInSamples(t)`
            if (shouldUpdateBounds || significantTimeChange) {
                updateEventBoundaries(t);
            }

            if constexpr (WithStretch) {
                // TODO: Account for fractional samples here by running an accumulator
                auto const numSourceSamples = std::clamp(static_cast<size_t>((double) numSamples / stretchFactor.load()), (size_t) 0, scratchBuffer.size());

                // Clear and read
                std::fill_n(scratchData, numSourceSamples, FloatType(0));

                readers[0].readAdding(scratchData, numSourceSamples);
                readers[1].readAdding(scratchData, numSourceSamples);

                stretch.process(&scratchData, numSourceSamples, &outputData, numSamples);
            } else {
                // Clear and read
                std::fill_n(outputData, numSamples, FloatType(0));

                readers[0].readAdding(outputData, numSamples);
                readers[1].readAdding(outputData, numSamples);
            }
        }

        using Sequence = std::map<double, FloatType, std::less<double>>;

        RefCountedPool<Sequence> seqPool;
        SingleWriterSingleReaderQueue<std::shared_ptr<Sequence>> seqQueue;
        std::shared_ptr<Sequence> activeSeq;

        typename Sequence::iterator prevEvent;
        typename Sequence::iterator nextEvent;

        SingleWriterSingleReaderQueue<SharedResourceBuffer<FloatType>> bufferQueue;
        SharedResourceBuffer<FloatType> activeBuffer;

        std::array<detail::BufferReader<FloatType>, 2> readers;
        size_t activeReader = 0;
        int64_t nextExpectedBlockStart = 0;

        std::atomic<double> sampleDuration = 0;
        double rtSampleDuration = 0;

        signalsmith::stretch::SignalsmithStretch<FloatType> stretch;
        std::atomic<double> stretchFactor = 1.0;
        std::vector<FloatType> scratchBuffer;
    };

    template <typename FloatType>
    using SampleSeqWithStretchNode = SampleSeqNode<FloatType, true>;

} // namespace elem
