#pragma once

#include "../GraphNode.h"


namespace elem
{

    // A simple identity node for midi events.
    //
    // Essentially the equivalent of the Identity node (el.in) for audio
    // signal processing.
    template <typename FloatType>
    struct MidiNoteInNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            ctx.inputEvents.template processEventsOfType<MidiEvent>([&](size_t time, MidiEvent const& event) {
                ctx.outputEvents.addEvent(time, MidiEvent(event));
            });
        }
    };

    // Maps incoming MidiEvents to AssignedMidiEvents with polyphonic
    // voice assignment.
    //
    // This uses MPE style voice allocation; where assigned voice is designated
    // by channel number. That means that we clobber the incoming channel number
    // assigned to the original event and rewrite it with a new number corresponding
    // to the assigned voice.
    template <typename FloatType>
    struct MidiNoteAllocateNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "voices") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                // Supports 1-16 voices
                auto v = static_cast<size_t>(std::min(16.0, std::max(1.0, (js::Number) val)));
                numVoices.store(v);
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            ctx.inputEvents.template processEventsOfType<MidiEvent>([&](size_t time, MidiEvent const& event) {
                auto* bytes = event.message.data();

                if (event.message.isNoteOn()) {
                    auto voiceIndex = getFreeVoice();
                    auto outEvent = MidiEvent((bytes[0] & 0xf0) | static_cast<uint8_t>(voiceIndex), bytes[1], bytes[2]);

                    ctx.outputEvents.addEvent(time, std::move(outEvent));
                    voiceMap[voiceIndex] = bytes[1];
                }

                if (event.message.isNoteOff()) {
                    auto assignedVoice = std::find(voiceMap.begin(), voiceMap.end(), bytes[1]);

                    if (assignedVoice != voiceMap.end()) {
                        auto voiceIndex = std::distance(voiceMap.begin(), assignedVoice);
                        auto outEvent = MidiEvent((bytes[0] & 0xf0) | static_cast<uint8_t>(voiceIndex), bytes[1], bytes[2]);

                        ctx.outputEvents.addEvent(time, std::move(outEvent));
                        // Clear the mapping
                        //
                        // TODO: Technically 0 is a valid midi note; maybe just use an int
                        // and let -1 be "unallocated"?
                        voiceMap[voiceIndex] = 0;
                    }
                }
            });
        }

        size_t getFreeVoice() {
            auto out = nextFreeVoice;

            // TODO: This is round robin, need better
            if (++nextFreeVoice >= (numVoices.load() - 1))
                nextFreeVoice = 0;

            return out;
        }

        // Maps the ith voice to the ith position in the array, where we capture
        // the note value the voice was last assigned
        std::array<uint8_t, 16> voiceMap;
        std::atomic<size_t> numVoices = 1;
        size_t nextFreeVoice = 0;
    };

    template <typename FloatType>
    struct MidiNoteUnpackNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            // TODO: Filter by channel too?
            if (key == "voice") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                auto v = static_cast<size_t>(std::max(0.0, (js::Number) val));
                targetVoiceIndex.store(v);
                filterByVoice.store(true);
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            size_t framesProcessed = 0;
            auto voiceFilter = filterByVoice.load();
            auto voiceIndex = targetVoiceIndex.load();

            ctx.inputEvents.template processEventsOfType<MidiEvent>([&](size_t time, MidiEvent const& event) {
                if (time >= ctx.numSamples)
                    return;

                if (!event.message.isNoteOn() && !event.message.isNoteOff())
                    return;

                if (voiceFilter && (voiceIndex != event.message.getChannel0to15()))
                    return;

                auto framesRemaining = ctx.numSamples - framesProcessed;
                std::fill_n(ctx.outputData[0] + framesProcessed, framesRemaining, noteFreq);

                if (ctx.numOutputChannels > 1) {
                    std::fill_n(ctx.outputData[1] + framesProcessed, framesRemaining, noteVelocity);
                }

                noteFreq = event.message.getNoteNumber().getFrequency();
                noteVelocity = event.message.isNoteOff()
                    ? FloatType(0)
                    : event.message.getVelocity() / (FloatType) 127;

                framesProcessed = time;
            });

            auto framesRemaining = ctx.numSamples - framesProcessed;
            std::fill_n(ctx.outputData[0] + framesProcessed, framesRemaining, noteFreq);

            if (ctx.numOutputChannels > 1) {
                std::fill_n(ctx.outputData[1] + framesProcessed, framesRemaining, noteVelocity);
            }
        }

        std::atomic<size_t> targetVoiceIndex = 0;
        std::atomic<bool> filterByVoice = false;

        FloatType noteFreq = 0;
        FloatType noteVelocity = 0;
    };

} // namespace elem
