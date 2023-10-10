#pragma once

#include "../GraphNode.h"
#include "../SingleWriterSingleReaderQueue.h"

#include "helpers/Change.h"
#include "helpers/RefCountedPool.h"

#include <optional>
#include <variant>


namespace elem
{

    template <typename FloatType>
    struct SparSeq2Node : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "seq") {
                if (!val.isArray())
                    return ReturnCode::InvalidPropertyType();


                auto& seq = val.getArray();

                if (seq.size() <= 0)
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

            if (key == "interpolate") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                interpOrder.store(static_cast<int32_t>((js::Number) val));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void updateEventBoundaries(double t) {
            nextEvent = activeSeq->upper_bound(t);

            // The next event is the first one in the sequence
            if (nextEvent == activeSeq->begin()) {
                prevEvent = activeSeq->end();
                return;
            }

            prevEvent = std::prev(nextEvent);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;
            auto const interp = interpOrder.load() == 1;

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
            if (numChannels < 1 || activeSeq == nullptr)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // We reference this a lot
            auto const seqEnd = activeSeq->end();

            // Helpers to add some tolerance to the time checks
            auto const before = [](double t1, double t2) { return t1 <= (t2 + 1e-9); };
            auto const after = [](double t1, double t2) { return t1 >= (t2 - 1e-9); };

            for (size_t i = 0; i < numSamples; ++i) {
                auto const t = static_cast<double>(inputData[0][i]);
                auto const shouldUpdateBounds = (prevEvent == seqEnd && nextEvent == seqEnd)
                    || (prevEvent != seqEnd && before(t, prevEvent->first))
                    || (nextEvent != seqEnd && after(t, nextEvent->first));

                if (shouldUpdateBounds) {
                    updateEventBoundaries(t);
                }

                // Now, if we still don't have a prevEvent, also output 0s
                if (prevEvent == seqEnd) {
                    outputData[i] = FloatType(0);
                    continue;
                }

                // If we don't have a nextEvent but do have a prevEvent, we output the prevEvent value indefinitely
                if (nextEvent == seqEnd) {
                    outputData[i] = prevEvent->second;
                    continue;
                }

                // Finally, here we have both bounds and can output accordingly
                double const alpha = interp ? ((t - prevEvent->first) / (nextEvent->first - prevEvent->first)) : 0.0;
                auto const out = prevEvent->second + FloatType(alpha) * (nextEvent->second - prevEvent->second);

                outputData[i] = out;
            }
        }

        using Sequence = std::map<double, FloatType, std::less<double>>;

        RefCountedPool<Sequence> seqPool;
        SingleWriterSingleReaderQueue<std::shared_ptr<Sequence>> seqQueue;
        std::shared_ptr<Sequence> activeSeq;

        typename Sequence::iterator prevEvent;
        typename Sequence::iterator nextEvent;

        std::atomic<double> interpOrder { 0 };
    };

} // namespace elem
