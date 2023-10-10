#pragma once

#include "../GraphNode.h"
#include "../SingleWriterSingleReaderQueue.h"

#include "helpers/Change.h"
#include "helpers/RefCountedPool.h"


namespace elem
{

    template <typename FloatType>
    struct RootNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int getChannelNumber()
        {
            return channelIndex.load();
        }

        FloatType getTargetGain()
        {
            return targetGain.load();
        }

        bool stillRunning()
        {
            auto const t = targetGain.load();
            auto const c = currentGain.load();

            return (t >= 0.5 || (std::abs(c - t) >= std::numeric_limits<FloatType>::epsilon()));
        }

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "active") {
                if (!val.isBool())
                    return ReturnCode::InvalidPropertyType();

                targetGain.store(FloatType(val ? 1 : 0));
            }

            if (key == "channel") {
                channelIndex.store(static_cast<int>((js::Number) val));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            auto const t = targetGain.load();
            auto c = currentGain.load();

            auto const direction = (t < c) ? FloatType(-1) : FloatType(1);
            auto const step = direction * FloatType(20) / FloatType(GraphNode<FloatType>::getSampleRate());

            for (size_t i = 0; i < numSamples; ++i) {
                outputData[i] = inputData[0][i] * c;
                c = std::clamp(c + step, FloatType(0), FloatType(1));
            }

            currentGain.store(c);
        }

        std::atomic<FloatType> targetGain = 1;
        std::atomic<FloatType> currentGain = 0;
        std::atomic<int> channelIndex = -1;
    };

    template <typename FloatType, bool WithReset = false>
    struct PhasorNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        FloatType tick (FloatType freq) {
            FloatType step = freq * (FloatType(1.0) / FloatType(GraphNode<FloatType>::getSampleRate()));
            FloatType y = phase;

            FloatType next = phase + step;
            phase = next - std::floor(next);

            return y;
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            if constexpr (WithReset) {
                // If we don't have the inputs we need, we bail here and zero the buffer
                // hoping to prevent unexpected signals.
                if (numChannels < 2)
                    return (void) std::fill_n(outputData, numSamples, FloatType(0));

                // The seocnd input in this mode is for hard syncing our phasor back
                // to 0 when the reset signal goes high
                for (size_t i = 0; i < numSamples; ++i) {
                    auto const xn = inputData[0][i];

                    if (change(inputData[1][i]) > FloatType(0.5)) {
                      phase = FloatType(0);
                    }

                    outputData[i] = tick(xn);
                }
            } else {
                // If we don't have the inputs we need, we bail here and zero the buffer
                // hoping to prevent unexpected signals.
                if (numChannels < 1)
                    return (void) std::fill_n(outputData, numSamples, FloatType(0));

                for (size_t i = 0; i < numSamples; ++i) {
                    outputData[i] = tick(inputData[0][i]);
                }
            }
        }

        Change<FloatType> change;
        FloatType phase = 0;
    };

    template <typename FloatType>
    struct ConstNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "value") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                value.store(FloatType((js::Number) val));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto* outputData = ctx.outputData;
            auto numSamples = ctx.numSamples;

            auto const v = value.load();

            for (size_t i = 0; i < numSamples; ++i) {
                outputData[i] = v;
            }
        }

        static_assert(std::atomic<FloatType>::is_always_lock_free);
        std::atomic<FloatType> value = 1;
    };

    template <typename FloatType>
    struct SampleRateNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto* outputData = ctx.outputData;
            auto numSamples = ctx.numSamples;

            for (size_t i = 0; i < numSamples; ++i) {
                outputData[i] = FloatType(GraphNode<FloatType>::getSampleRate());
            }
        }
    };

    template <typename FloatType>
    struct CounterNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            for (size_t i = 0; i < numSamples; ++i) {
                auto const in = inputData[0][i];

                // When the gate is high, we just keep counting on a sample by sample basis
                if ((FloatType(1.0) - in) <= std::numeric_limits<FloatType>::epsilon()) {
                    outputData[i] = count;
                    count = count + FloatType(1);
                    continue;
                }

                // When the gate is closed, we reset
                count = FloatType(0);
                outputData[i] = FloatType(0);
            }
        }

        FloatType count = 0;
    };

    // Simply accumulates its input until reset
    template <typename FloatType>
    struct AccumNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 2)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            for (size_t i = 0; i < numSamples; ++i) {
                auto const in = inputData[0][i];
                auto const reset = inputData[1][i];

                if (change(reset) > FloatType(0.5)) {
                  runningTotal = FloatType(0);
                }

                runningTotal += in;
                outputData[i] = runningTotal;
            }
        }

        Change<FloatType> change;
        FloatType runningTotal = 0;
    };

    template <typename FloatType>
    struct LatchNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // We need the first channel to be the latch signal and the second channel to
            // represent the signal we want to sample when the latch changes
            if (numChannels < 2)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            for (size_t i = 0; i < numSamples; ++i) {
                const FloatType l = inputData[0][i];
                const FloatType x = inputData[1][i];
                const FloatType eps = std::numeric_limits<FloatType>::epsilon();

                // Check if it's time to latch
                // We're looking for L[i-1] near zero and L[i] non-zero, where L
                // is the discrete clock signal used to identify the hold. We latch
                // on the positive-going transition from a zero value to a non-zero
                // value.
                if (std::abs(z) <= eps && l > eps) {
                    hold = x;
                }

                z = l;
                outputData[i] = hold;
            }
        }

        FloatType z = 0;
        FloatType hold = 0;
    };

    template <typename FloatType>
    struct MaxHold : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "hold") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                auto h = GraphNode<FloatType>::getSampleRate() * 0.001 * (js::Number) val;
                holdTimeSamples.store(static_cast<uint32_t>(h));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // We want two input signals: the input to monitor and the reset signal
            if (numChannels < 2)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            auto const hts = holdTimeSamples.load();

            for (size_t i = 0; i < numSamples; ++i) {
                const FloatType in = inputData[0][i];
                const FloatType reset = inputData[1][i];

                if (change(reset) > FloatType(0.5) || ++samplesAtCurrentMax >= hts) {
                    max = in;
                    samplesAtCurrentMax = 0;
                } else {
                    if (in > max) {
                      samplesAtCurrentMax = 0;
                      max = in;
                    }
                }

                outputData[i] = max;
            }
        }

        Change<FloatType> change;
        std::atomic<uint32_t> holdTimeSamples = std::numeric_limits<uint32_t>::max();
        uint32_t samplesAtCurrentMax = 0;
        FloatType max = 0;
    };

    // This is a simple utility designed to act like a gate for pulse signals, except
    // that the gate closes immediately after letting a single pulse through.
    //
    // The use case here is for quantized events: suppose you want to trigger a sample
    // on the next quarter-note, you can arm the OnceNode and feed it a quarter-note rate
    // pulse train. No matter when you arm it, the _next_ pulse will be the one through,
    // and to repeat the behavior you need to re-arm (i.e. set arm = false, then arm = true).
    template <typename FloatType>
    struct OnceNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "arm") {
                if (!val.isBool())
                    return ReturnCode::InvalidPropertyType();


                // We don't let the incoming prop actually disarm.
                if (!armed.load()) {
                    armed.store((bool) val);
                }
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            if (numChannels < 1)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            auto const isArmed = armed.load();

            for (size_t i = 0; i < numSamples; ++i) {
                auto const delta = change(inputData[0][i]);
                auto const risingEdge = delta > FloatType(0.5);
                auto const fallingEdge = delta < FloatType(-0.5);

                // On the first rising edge when armed, we gain to 1 and disarm. We
                // will then let through the complete pulse, setting gain back to 0
                // on the falling edge of that pulse.
                if (isArmed && risingEdge) {
                    gain = FloatType(1);
                    armed.store(false);
                }

                if (fallingEdge) {
                    gain = FloatType(0);
                }

                outputData[i] = inputData[0][i] * gain;
            }
        }

        static_assert(std::atomic<FloatType>::is_always_lock_free);
        std::atomic<FloatType> armed = false;
        FloatType gain = 0;
        Change<FloatType> change;
    };


    template <typename FloatType>
    struct SequenceNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "hold") {
                if (!val.isBool())
                    return ReturnCode::InvalidPropertyType();

                wantsHold.store((js::Boolean) val);
            }

            if (key == "loop") {
                if (!val.isBool())
                    return ReturnCode::InvalidPropertyType();

                wantsLoop.store((js::Boolean) val);
            }

            if (key == "offset") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                if ((js::Number) val < 0.0)
                    return ReturnCode::InvalidPropertyValue();

                seqOffset.store(static_cast<size_t>((js::Number) val));
            }

            if (key == "seq") {
                if (!val.isArray())
                    return ReturnCode::InvalidPropertyType();

                auto& seq = val.getArray();
                auto data = sequencePool.allocate();

                // The data array that we get from the pool may have been
                // previously used to represent a different sequence. Need to
                // resize here and then overwrite below.
                data->resize(seq.size());

                for (size_t i = 0; i < seq.size(); ++i)
                    data->at(i) = FloatType((js::Number) seq[i]);

                // Finally, we push our new sequence data into the event
                // queue for the realtime thread.
                sequenceQueue.push(std::move(data));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // First order of business: grab the most recent sequence buffer to use if
            // there's anything in the queue
            if (sequenceQueue.size() > 0) {
                while (sequenceQueue.size() > 0) {
                    sequenceQueue.pop(activeSequence);
                }

                // Here we want to catch the case where the new sequence we pulled in has a different
                // length. If the new length is smaller than the prior length, we modulo our current read
                // index around, attempting to provide a nice default behavior. Otherwise, the index
                // remains unchanged.
                //
                // If the user needs specific behavior, they can achieve it by setting the offset property
                // and employing the reset train.
                seqIndex = (seqIndex % activeSequence->size());

                // Next, we want to catch the case where the user is changing sequences in the middle
                // of emitting values with hold: true. In that case, we need to update our holdValue immediately
                // to sample from the new sequence data to reflect the change, otherwise they won't hear the
                // new sequence values until the next rising edge of the trigger train.
                //
                // Only do this if we've already initiated the trigger sequence, otherwise we'll move
                // our train ahead too early (i.e. the first time seq data comes in but before the first pulse comes in)
                if (hasReceivedFirstPulse) {
                  holdValue = activeSequence->at(seqIndex);
                }
            }

            // Next, if we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1 || activeSequence == nullptr)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // We let the user optionally supply a second input, a pulse train whose
            // rising edge will reset our sequence position back to the start
            bool hasResetSignal = numChannels > 1;
            bool const hold = wantsHold.load();
            bool const loop = wantsLoop.load();

            for (size_t i = 0; i < numSamples; ++i) {
                auto const in = inputData[0][i];
                auto const reset = hasResetSignal ? inputData[1][i] : FloatType(0);

                // Reset our sequence index if we're on the rising edge of the reset signal
                //
                // We want this to happen before checking the trigger train so that if we reeive
                // a reset and a trigger at exactly the same time, we trigger from the reset index.
                if (resetChange(reset) > FloatType(0.5)) {
                    // We snap our seqIndex back to the offset at the time of the reset edge but
                    // we don't touch our holdValue until the next rising edge of the trigger input.
                    //
                    // This can yield a situation like the following: imagine a couple of small (low resolution)
                    // sequences playing half-note chords in a synth. Suppose I'm halfway through bar 2 playing
                    // a Dm7 and I want to drop my timeline/playhead a quarter of the way through bar 1 where I have
                    // an Am7 programmed in. The reset fires right away, but we won't take the new values of our chord
                    // sequences until the _next_ rising edge of the pulse train, i.e. at the boundary of the next half
                    // note. So we won't hear our Am7 (and in the interrim might continue hearing our Dm7) until we get
                    // to the next chord in the chord progression.
                    //
                    // This is undesirable, of course, but can be solved conceptually by using a massively high resolution
                    // sequence. That way I might not pick up the Am7 at exactly the right point in sampleTime but I'll pick
                    // it up maybe 1/256th note later (so quickly that the difference would be imperceptible). This is conceptually
                    // a totally acceptable behavior for el.seq but comes at the cost of using super high resolution sequences.
                    // Perhaps we need a new node for defining high resolution sequences without massive arrays...
                    seqIndex = seqOffset.load();
                }

                // Increment our sequence index on the rising edge of the input signal
                if (change(in) > FloatType(0.5)) {
                    holdValue = activeSequence->at(std::min(seqIndex, activeSequence->size() - 1));
                    hasReceivedFirstPulse = true;

                    // When looping we wrap around back to 0
                    if ((++seqIndex >= activeSequence->size()) && loop) {
                        seqIndex = 0;
                    }
                }

                // Now, if our seqIndex has run past the end of the array, we either
                //  * Emit 0 if hold = false
                //  * Emit the last value in the seq array if hold = true
                //
                // Finally, when holding, we don't fall to 0 with the incoming pulse train.
                if (seqIndex < activeSequence->size()) {
                    outputData[i] = (hold ? holdValue : holdValue * in);
                } else {
                    outputData[i] = (hold ? holdValue : 0);
                }
            }
        }

        using SequenceData = std::vector<FloatType>;

        RefCountedPool<SequenceData> sequencePool;
        SingleWriterSingleReaderQueue<std::shared_ptr<SequenceData>> sequenceQueue;
        std::shared_ptr<SequenceData> activeSequence;

        Change<FloatType> change;
        Change<FloatType> resetChange;

        std::atomic<bool> wantsHold = false;
        std::atomic<bool> wantsLoop = true;
        std::atomic<size_t> seqOffset = 0;

        FloatType holdValue = 0;
        size_t seqIndex = 0;
        bool hasReceivedFirstPulse = false;
    };

} // namespace elem
