#pragma once

#include "../GraphNode.h"
#include "../SingleWriterSingleReaderQueue.h"

#include "helpers/Change.h"
#include "helpers/RefCountedPool.h"


namespace elem
{

    // The Seq2 node is a simple variant on the original SequenceNode with a
    // different implementation that is vastly simpler and more robust, but
    // which differs in behavior enough to warrant a new node rather than a change.
    //
    // The primary difference is that Seq2 accepts new offset values at any time
    // which take immediate effect, whereas the original Seq will only reset to
    // its prescribed offset value at the time of a rising edge in the reset signal.
    //
    // The problem with this older behavior is that the reconciler makes no guarantees
    // about ordering of events when performing a given render pass. We might set one Seq
    // node's offset value, then set some const node value to 1 indicating the rising edge
    // of a reset train, then set another Seq node's offset. The GraphHost just executes these
    // calls in order on the non-realtime thread while the realtime thread freely runs, which
    // means it's plenty possible for the first Seq here to receive its offset and then the
    // edge of the reset train while the second Seq receives the reset train and then its new
    // offset– but it already reset to some stale value and won't reset again until the next
    // rising edge.
    //
    // So here we simply count elapsed rising edges and continuously index into the active
    // sequence array summing in the current offset. Resets set our elapsed count to 0, and
    // new offset values are taken immediately (block level).
    template <typename FloatType>
    struct Seq2Node : public GraphNode<FloatType> {
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
            auto const offset = seqOffset.load();

            for (size_t i = 0; i < numSamples; ++i) {
                auto const in = inputData[0][i];
                auto const reset = hasResetSignal ? inputData[1][i] : FloatType(0);

                // Increment our sequence index on the rising edge of the input signal
                if (change(in) > FloatType(0.5)) {
                    edgeCount++;
                }

                // Reset our edge count if we're on the rising edge of the reset signal
                //
                // We want this to happen after checking the trigger train so that if we receive
                // a reset and a trigger at exactly the same time, we trigger from the reset index.
                if (resetChange(reset) > FloatType(0.5)) {
                    edgeCount = 0;
                }

                // Now, if our seqIndex has run past the end of the array, we either
                //  * Emit 0 if hold = false
                //  * Emit the last value in the seq array if hold = true
                //  * Loop around using the modulo operator if loop = true
                auto const seqSize = activeSequence->size();
                auto const seqIndex = offset + edgeCount;

                auto const nextOut = (seqIndex < seqSize)
                    ? activeSequence->at(seqIndex)
                    : (loop
                        ? activeSequence->at(seqIndex % seqSize)
                        : (hold
                            ? activeSequence->at(seqSize - 1)
                            : FloatType(0)));

                // Finally, when holding, we don't fall to 0 with the incoming pulse train.
                outputData[i] = (hold ? nextOut : nextOut * in);
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

        // The number of rising edges counted since the last reset event, or
        // since the beginning of this node's life.
        size_t edgeCount = 0;
    };

} // namespace elem
