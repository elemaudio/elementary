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
    struct SparSeqNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;
        using SequenceData = std::map<int32_t, FloatType>;

        // Here we follow a pattern just like the larger GraphRenderer event queue pattern for
        // moving change events from the non-realtime thread into the realtime thread safely
        // without locking. We only have two event types: one for new sequence data, and one for
        // a pair of new loop points. The EmptyEvent primarily just enables default construction
        // into an event type that is neither of the two we care about.
        struct EmptyEvent {};

        struct NewSequenceEvent {
            std::shared_ptr<SequenceData> sequence;
        };

        struct NewLoopPointsEvent {
            int32_t loopStart;
            int32_t loopEnd;
        };

        // Our ChangeEvent union type.
        using ChangeEvent = std::variant<EmptyEvent, NewSequenceEvent, NewLoopPointsEvent>;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "offset") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                if ((js::Number) val < 0.0)
                    return ReturnCode::InvalidPropertyValue();

                seqOffset.store(static_cast<size_t>((js::Number) val));
            }

            if (key == "loop") {
                if (val.isNull() || (val.isBool() && !val)) {
                    changeEventQueue.push(ChangeEvent { NewLoopPointsEvent { -1, -1 } });
                } else {
                    if (!val.isArray())
                        return ReturnCode::InvalidPropertyType();

                    auto& points = val.getArray();

                    changeEventQueue.push(ChangeEvent { NewLoopPointsEvent {
                        static_cast<int32_t>((js::Number) points[0]),
                        static_cast<int32_t>((js::Number) points[1]),
                    }});
                }
            }

            if (key == "follow") {
                if (!val.isBool())
                    return ReturnCode::InvalidPropertyType();

                followAction.store((bool) val);
            }

            if (key == "interpolate") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                holdOrder.store(static_cast<int32_t>((js::Number) val));
            }

            // The estimated period of the incoming clock cycle in seconds. Used to improve the resolution
            // of the interpolation coefficient. Ignored if using zero order hold (no interpolation).
            if (key == "tickInterval") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                auto const ti = (js::Number) val;

                if (ti < 0.0)
                    return ReturnCode::InvalidPropertyValue();

                auto const tickIntervalSamples = GraphNode<FloatType>::getSampleRate() *  ti;
                tickInterval.store(tickIntervalSamples);
            }

            if (key == "seq") {
                if (!val.isArray())
                    return ReturnCode::InvalidPropertyType();

                auto& seq = val.getArray();
                auto data = sequencePool.allocate();

                // The data array that we get from the pool may have been
                // previously used to represent a different sequence.
                data->clear();

                // We expect from the JavaScript side an array of event objects, where each
                // event includes a value to take and a 32-bit int tick time at which to take that value.
                for (size_t i = 0; i < seq.size(); ++i) {
                    auto& event = seq[i].getObject();

                    FloatType value = static_cast<FloatType>((js::Number) event.at("value"));
                    int32_t time = static_cast<int32_t>((js::Number) event.at("tickTime"));

                    data->insert({time, value});
                }

                // Finally, we push our new sequence data into the event
                // queue for the realtime thread.
                changeEventQueue.push(ChangeEvent { NewSequenceEvent { std::move(data) } });
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        typename SequenceData::iterator findTickValue(int32_t tickTime) {
            // Look up the value we should take by considering where the counter
            // is in relation to our sparsely defined sequence. We find the first event _after_
            // the current time using upper_bound, then step back once to find the value we
            // should take currently.
            auto it = activeSequence->upper_bound(tickTime);

            // If we get back the beginning of the map, that means that either
            //   (1) the first entry specifies a value for a time that we haven't reached yet,
            //       in which case we just stay silent. Or,
            //   (2) the first entry defines a value for tickTime 0, in which case we take it
            if (it == activeSequence->begin()) {
                if (it->first == 0) {
                    return it;
                }

                return activeSequence->end();
            }

            return --it;
        }

        int32_t getTickTime(int32_t offset) {
            auto tickTime = static_cast<int32_t>(offset) + edgeCount;

            // If we're looping and we've walked past the end of the loop, we have to wrap back
            // around the tickTime before the lookup
            auto const ls = loopPoints.start;
            auto const le = loopPoints.end;

            if ((ls > -1) && (le > -1) && (tickTime >= le)) {
                auto const loopDuration = le - ls;

                if (loopDuration > 0) {
                    if (pendingLoopPoints) {
                        // Here, if we have pending loop points that means we didn't immediately promote
                        // them (i.e. follow = true), thus we're waiting for the end of the current loop to promote.
                        // In that case, we jump into the new loop points and promote right here.
                        loopPoints = *pendingLoopPoints;
                        pendingLoopPoints = std::nullopt;

                        // New loop points, post promotion
                        auto const nls = loopPoints.start;
                        auto const nle = loopPoints.end;

                        // If the promoted tick points indicate no looping, then we stop here.
                        if ((nls == -1) && (nle == -1)) {
                            return tickTime;
                        }

                        tickTime = nls + ((tickTime - le) % (nle - nls));
                    } else {
                        // Otherwise, we step into the existing loop points
                        tickTime = ls + ((tickTime - le) % loopDuration);
                    }

                    // This was previously the `resetAtLoopBoundaries` which meant that we clobber the edgeCount
                    // when resetting if and only if the user has requested it. I think it's pretty odd behavior if this _isn't_
                    // set, where the edgeCount runs off to some large number in the future while you're looping through
                    // your sequences. Without this, then any time you go from looping to not looping you would jump off into the
                    // future wherever the edgeCount is. Technically this is a breaking change to make this behavior the default,
                    // but now is the time.
                    edgeCount = tickTime - static_cast<int32_t>(offset);
                }
            }

            return tickTime;
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // We let the user optionally supply a second input, a pulse train whose
            // rising edge will reset our sequence position back to the start
            bool hasResetSignal = numChannels > 1;

            auto const offset = seqOffset.load();
            auto const follow = followAction.load();
            auto const ho = holdOrder.load();
            auto const samplesPerClockCycle = tickInterval.load();

            // Current sequence "time" in ticks
            auto tickTime = getTickTime(offset);

            // First order of business: grab the most recent sequence buffer to use if
            // there's anything in the queue
            if (changeEventQueue.size() > 0) {
                while (changeEventQueue.size() > 0) {
                    ChangeEvent nextEvent;
                    changeEventQueue.pop(nextEvent);

                    std::visit([this](auto && evt) {
                        using EventType = std::decay_t<decltype(evt)>;

                        if constexpr (std::is_same_v<EventType, NewSequenceEvent>) {
                            activeSequence = std::move(evt.sequence);
                        }

                        if constexpr (std::is_same_v<EventType, NewLoopPointsEvent>) {
                            pendingLoopPoints = LoopPoints { evt.loopStart, evt.loopEnd };
                        }
                    }, nextEvent);
                }

                // New sequence, but our internal count state is maintained so we immediately
                // perform a lookup.
                holdValue = findTickValue(tickTime);
            }

            // If after draining the changeEventQueue we have pending loop points, then here we
            // consider whether or not to immediately apply them. Generally, if we are already looping
            // and receiving new points, we only want to apply them immediately if !follow. However, regardless
            // of follow behavior, if we aren't already looping then we start immediately.
            if (pendingLoopPoints) {
                bool const takeImmediately = ((loopPoints.start == -1) && (loopPoints.end == -1)) || !follow;

                if (takeImmediately) {
                    loopPoints = *pendingLoopPoints;
                    pendingLoopPoints = std::nullopt;
                    tickTime = getTickTime(offset);
                }
            }

            // Next, if we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1 || activeSequence == nullptr)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            for (size_t i = 0; i < numSamples; ++i) {
                samplesSinceClockEdge++;

                auto const in = inputData[0][i];
                auto const reset = hasResetSignal ? inputData[1][i] : FloatType(0);

                auto const isTriggerEdge = change(in) > FloatType(0.5);
                auto const isResetEdge = resetChange(reset) > FloatType(0.5);

                // Reset our edge count if we're on the rising edge of the reset signal
                if (isResetEdge) {
                    edgeCount = 0;
                }

                // Increment our sequence index on the rising edge of the input signal
                if (isTriggerEdge) {
                    // We have a special case here; if we have a reset and a trigger edge at the exact
                    // same sample, then we trigger from an edgeCount of 0. Else, we increment our count.
                    edgeCount = isResetEdge ? 0 : edgeCount + 1;

                    // Reset our sample count
                    samplesSinceClockEdge = 0;

                    // Update the tick time given the new edgeCount
                    tickTime = getTickTime(offset);

                    // And update our current hold
                    holdValue = findTickValue(tickTime);
                }

                // Invalid hold value; output zeros
                if (holdValue == activeSequence->end()) {
                    outputData[i] = FloatType(0);
                    continue;
                }

                switch (ho) {
                    case 1:
                    {
                        auto holdRight = std::next(holdValue);

                        // Linear interpolation between two values. If our RHS is off the end of the container
                        // then we just return the last value in the sequence.
                        if (holdRight == activeSequence->end()) {
                            outputData[i] = holdValue->second;
                            break;
                        }

                        auto const tl = holdValue->first;
                        auto const tr = holdRight->first;
                        auto const leftValue = holdValue->second;
                        auto const rightValue = holdRight->second;

                        // This gets us linear interp but still stair-stepped according to the clock edge.
                        double alpha = (double) std::max(0, tickTime - tl) / (double) (tr - tl);

                        // We can improve our linear interpolation coefficient if we have an estimate for how many
                        // samples to expect per clock cycle.
                        //
                        // However, we're careful here not to interpolate _past_ one clock cycle based on elapsed
                        // samples. Otherwise the clock may have stopped running but we'll keep moving the output
                        // signal based on elapsed samples.
                        if (samplesPerClockCycle > 0.0) {
                            alpha += (std::min((double) samplesSinceClockEdge, samplesPerClockCycle) / samplesPerClockCycle) / (double) (tr - tl);
                        }

                        outputData[i] = leftValue + alpha * (rightValue - leftValue);
                        break;
                    }
                    case 0:
                    default:
                    {
                        outputData[i] = holdValue->second;
                        break;
                    }
                }
            }
        }

        RefCountedPool<SequenceData> sequencePool;
        SingleWriterSingleReaderQueue<ChangeEvent> changeEventQueue;
        std::shared_ptr<SequenceData> activeSequence;

        Change<FloatType> change;
        Change<FloatType> resetChange;

        struct LoopPoints {
            int32_t start = -1;
            int32_t end = -1;
        };

        LoopPoints loopPoints;
        std::optional<LoopPoints> pendingLoopPoints;

        std::atomic<size_t> seqOffset = 0;
        std::atomic<bool> followAction { false };

        // The number of rising edges counted since the last reset event, or
        // since the beginning of this node's life.
        //
        // We start here at -1 because the _first_ rising edge of the incoming
        // clock signal should trigger the sequence at tickTime 0.
        int32_t edgeCount = -1;

        // The number of elapsed samples counted since the last clock edge.
        size_t samplesSinceClockEdge = 0;

        // Because our sparse lookup uses a binary search, we only want to do it
        // when we absolutely have to. The rest of the time, we just hold that value here.
        typename SequenceData::iterator holdValue;
        std::atomic<int32_t> holdOrder { 0 };
        std::atomic<double> tickInterval { 0 };

        static_assert(std::atomic<double>::is_always_lock_free);
    };

} // namespace elem
