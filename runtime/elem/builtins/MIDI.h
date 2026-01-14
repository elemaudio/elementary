#pragma once

#include "../BlockEvents.h"
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
    // This uses MPE style voice allocation, where assigned voice is designated
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
                    voiceMap[voiceIndex].note = static_cast<int32_t>(bytes[1]);
                    voiceMap[voiceIndex].lastModified = steadyClock + time;
                }

                if (event.message.isNoteOff()) {
                    auto assignedVoice = std::find_if(voiceMap.begin(), voiceMap.end(),
                        [&bytes](const Assignment& a) { return a.note == static_cast<int32_t>(bytes[1]); });

                    if (assignedVoice != voiceMap.end()) {
                        auto voiceIndex = std::distance(voiceMap.begin(), assignedVoice);
                        auto outEvent = MidiEvent((bytes[0] & 0xf0) | static_cast<uint8_t>(voiceIndex), bytes[1], bytes[2]);

                        ctx.outputEvents.addEvent(time, std::move(outEvent));

                        // Clear the mapping
                        voiceMap[voiceIndex].note = -1;
                        voiceMap[voiceIndex].lastModified = steadyClock + time;
                    }
                }

                if (event.message.isAllNotesOff()) {
                    // Propagate the event for downstream nodes
                    ctx.outputEvents.addEvent(time, MidiEvent(event));

                    // Deallocate all voices
                    std::for_each(voiceMap.begin(), voiceMap.end(), [this, &time](auto& voice) {
                        voice.note = -1;
                        voice.lastModified = steadyClock + time;
                    });
                }
            });

            steadyClock += ctx.numSamples;
        }

        size_t getFreeVoice() {
            // The first free voice is the one in voiceMap that has note == -1
            // and whose lastModified timestamp is the oldest (i.e. smallest value).
            auto nv = numVoices.load();
            size_t bestIndex = 0;
            int64_t oldestTime = std::numeric_limits<int64_t>::max();

            for (size_t i = 0; i < nv; ++i) {
                if (voiceMap[i].note == -1 && voiceMap[i].lastModified < oldestTime) {
                    bestIndex = i;
                    oldestTime = voiceMap[i].lastModified;
                }
            }

            // If we found a free voice, return it
            if (voiceMap[bestIndex].note == -1) {
                return bestIndex;
            }

            // If there is no free voice, we want whichever voiceMap entry has the
            // oldest lastModified timestamp, regardless of note value.
            for (size_t i = 0; i < nv; ++i) {
                if (voiceMap[i].lastModified < oldestTime) {
                    bestIndex = i;
                    oldestTime = voiceMap[i].lastModified;
                }
            }

            return bestIndex;
        }

        // Maps the ith voice to the ith position in the array, where we capture
        // the note value the voice was last assigned
        struct Assignment {
            int32_t note = -1;
            int64_t lastModified = 0;
        };

        std::array<Assignment, 16> voiceMap;
        std::atomic<size_t> numVoices = 16;
        int64_t steadyClock = 0;
    };

    // Maps incoming midi note events into audio signal data.
    //
    // This will constantly emit a pair of audio signals representing
    // the note frequency (in Hz) and note velocity (0-1) of the most
    // recent midi note that it has processed.
    template <typename FloatType>
    struct MidiNoteUnpackNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "channel") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                auto v = static_cast<int32_t>(std::max(0.0, (js::Number) val));
                channelFilter.store(v);
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            size_t framesProcessed = 0;
            int32_t const targetChannel = channelFilter.load();

            ctx.inputEvents.template processEventsOfType<MidiEvent>([&](size_t time, MidiEvent const& event) {
                if (time >= ctx.numSamples)
                    return;

                if (!event.message.isNoteOn() && !event.message.isNoteOff() && !event.message.isAllNotesOff())
                    return;

                if ((targetChannel >= 0) && (targetChannel != static_cast<int32_t>(event.message.getChannel0to15())))
                    return;

                auto framesRemaining = ctx.numSamples - framesProcessed;
                std::fill_n(ctx.outputData[0] + framesProcessed, framesRemaining, noteFreq);

                if (ctx.numOutputChannels > 1) {
                    std::fill_n(ctx.outputData[1] + framesProcessed, framesRemaining, noteVelocity);
                }

                if (event.message.isNoteOn()) {
                    noteFreq = event.message.getNoteNumber().getFrequency();
                    noteVelocity = event.message.getVelocity() / (FloatType) 127;
                }

                if (event.message.isNoteOff()) {
                    noteFreq = event.message.getNoteNumber().getFrequency();
                    noteVelocity = FloatType(0);
                }

                if (event.message.isAllNotesOff()) {
                    noteVelocity = FloatType(0);
                }

                framesProcessed = time;
            });

            auto framesRemaining = ctx.numSamples - framesProcessed;
            std::fill_n(ctx.outputData[0] + framesProcessed, framesRemaining, noteFreq);

            if (ctx.numOutputChannels > 1) {
                std::fill_n(ctx.outputData[1] + framesProcessed, framesRemaining, noteVelocity);
            }
        }

        std::atomic<int32_t> channelFilter = -1;
        FloatType noteFreq = 0;
        FloatType noteVelocity = 0;
    };

    // A simple pitch utility for midi events.
    //
    // For every incoming midi note event, this will nudge the associated
    // note value according to the steps property.
    template <typename FloatType>
    struct MidiNoteShiftNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "steps") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                auto v = static_cast<int32_t>((js::Number) val);
                steps.store(v);
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto const s = steps.load();

            ctx.inputEvents.template processEventsOfType<MidiEvent>([&](size_t time, MidiEvent const& event) {
                auto* bytes = event.message.data();
                auto newNoteValue = std::max(0, std::min(127, static_cast<int32_t>(bytes[1]) + s));
                auto shiftedEvent = MidiEvent(bytes[0], static_cast<uint8_t>(newNoteValue), bytes[2]);
                ctx.outputEvents.addEvent(time, std::move(shiftedEvent));
            });
        }

        std::atomic<int32_t> steps = 0;
    };

    // Maps incoming midi CC events into audio signal data.
    //
    // This will constantly emit an audio signal representing the value
    // of the most recent midi CC event that it has processed, filtered
    // by channel and control number.
    template <typename FloatType>
    struct MidiCCNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "channel") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                auto v = static_cast<int32_t>(std::max(0.0, (js::Number) val));
                channelFilter.store(v);
            }

            if (key == "control") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                auto v = static_cast<int32_t>(std::max(0.0, std::min(127.0, (js::Number) val)));
                controlFilter.store(v);
            }

            if (key == "normalize") {
                if (!val.isBool())
                    return ReturnCode::InvalidPropertyType();

                normalize.store((js::Boolean) val);
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            size_t framesProcessed = 0;
            int32_t const targetChannel = channelFilter.load();
            int32_t const targetControl = controlFilter.load();
            bool const shouldNormalize = normalize.load();

            ctx.inputEvents.template processEventsOfType<MidiEvent>([&](size_t time, MidiEvent const& event) {
                if (time >= ctx.numSamples)
                    return;

                if (!event.message.isController())
                    return;

                if ((targetChannel >= 0) && (targetChannel != static_cast<int32_t>(event.message.getChannel0to15())))
                    return;

                if ((targetControl >= 0) && (targetControl != static_cast<int32_t>(event.message.getControllerNumber())))
                    return;

                auto framesRemaining = ctx.numSamples - framesProcessed;
                std::fill_n(ctx.outputData[0] + framesProcessed, framesRemaining, ccValue);

                auto rawValue = static_cast<FloatType>(event.message.getControllerValue());
                ccValue = shouldNormalize ? (rawValue / FloatType(127)) : rawValue;

                framesProcessed = time;
            });

            auto framesRemaining = ctx.numSamples - framesProcessed;
            std::fill_n(ctx.outputData[0] + framesProcessed, framesRemaining, ccValue);
        }

        std::atomic<int32_t> channelFilter = -1;
        std::atomic<int32_t> controlFilter = -1;
        std::atomic<bool> normalize = false;
        FloatType ccValue = 0;
    };

} // namespace elem
