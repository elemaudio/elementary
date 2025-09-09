#pragma once

#include "../GraphNode.h"


namespace elem
{

    template <typename FloatType>
    struct MidiNoteInNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            // TODO: allow channel filtering? voice?
            // i.e. user sets a property here to tell us which notes to react to and
            // which to ignore
            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            size_t framesProcessed = 0;

            ctx.inputEvents.template processEventsOfType<MidiEvent>([&](size_t time, MidiEvent const& event) {
                if (time < ctx.numSamples && (event.message.isNoteOn() || event.message.isNoteOff())) {
                    auto framesRemaining = ctx.numSamples - framesProcessed;

                    std::fill_n(ctx.outputData[0] + framesProcessed, framesRemaining, noteFreq);

                    if (ctx.numOutputChannels > 1) {
                        std::fill_n(ctx.outputData[1] + framesProcessed, framesRemaining, noteVelocity);
                    }

                    noteFreq = event.message.getNoteNumber().getFrequency();
                    noteVelocity = event.message.getVelocity() / (FloatType) 127;
                    framesProcessed = time;
                }
            });

            auto framesRemaining = ctx.numSamples - framesProcessed;
            std::fill_n(ctx.outputData[0] + framesProcessed, framesRemaining, noteFreq);

            if (ctx.numOutputChannels > 1) {
                std::fill_n(ctx.outputData[1] + framesProcessed, framesRemaining, noteVelocity);
            }
        }

        FloatType noteFreq = 0;
        FloatType noteVelocity = 0;
    };

} // namespace elem
