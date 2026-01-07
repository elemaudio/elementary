#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>

#include "GainFade.h"
#include "elem/SharedResource.h"

namespace elem
{

    template <typename FloatType>
    struct BufferReader {
        BufferReader(double sampleRate, double fadeTime)
            : fade(sampleRate, fadeTime, fadeTime)
        {
        }

        void engage (double start, double currentTime, size_t _bufferSize) {
            startTime = start;
            bufferSize = _bufferSize;
            fade.fadeIn();

            position = static_cast<size_t>(((currentTime - startTime) / sampleDuration) * (double) (bufferSize - 1u));
            position = std::clamp<size_t>(position, 0, bufferSize);
        }

        void disengage() {
            fade.fadeOut();
        }

        template <typename DestType>
        struct ReadContext {
            SharedResource* source;
            DestType** outputData;
            size_t numChannels;
            size_t numSamples;
            std::optional<uint64_t> startOffset;
            std::optional<uint64_t> stopOffset;
            bool shouldLoop = false;
        };

        template <typename DestType>
        void readAdding(ReadContext<DestType> const& ctx) {
            if (ctx.source == nullptr || position < 0.0 || fade.fadedOut()) {
                return;
            }

            auto const numChannels = std::min(ctx.numChannels, ctx.source->numChannels());
            auto const bufferSize = ctx.source->numSamples();
            if (numChannels == 0 || bufferSize == 0) {
                return;
            }

            auto const _startOffset = ctx.startOffset.value_or(0);
            auto const _stopOffset = ctx.stopOffset.value_or(0);
            auto const startOffset = _startOffset >= 0 ? 
                std::min(_startOffset, static_cast<uint64_t>(bufferSize)) : 0;
            auto const stopOffset = _stopOffset >= 0 ? 
                std::min(_stopOffset, static_cast<uint64_t>(bufferSize)) : 0;

            auto pos = position;
            elem::GainFade<FloatType> localFade(fade);

            for (size_t j = 0; j < numChannels; ++j) {
                pos = position;
                localFade = fade;
                auto bufferView = ctx.source->getChannelData(j);
                auto* sourceData = bufferView.data();
                size_t const sourceLength = bufferView.size();
    
                for (size_t i = 0; i < ctx.numSamples; ++i) {
                    if (pos >= (double) (sourceLength - stopOffset)) {
                        if (!ctx.shouldLoop) {
                            break;
                        }
                        pos = (double) startOffset;
                    }
        
                    // Linear interpolation on the buffer read
                    auto readLeft = static_cast<size_t>(pos);
                    auto readRight = readLeft + 1;
                    auto const frac = FloatType(pos - (double) readLeft);
        
                    if (readLeft >= sourceLength)
                        readLeft -= sourceLength;
        
                    if (readRight >= sourceLength)
                        readRight -= sourceLength;

                    auto const left = sourceData[readLeft];
                    auto const right = sourceData[readRight];
        
                    // Now we can read the next sample out of the buffer with linear
                    // interpolation for sub-sample reads.
                    auto const out = localFade(left + frac * (right - left));
                    ctx.outputData[j][i] += static_cast<DestType>(out);
                    ++pos;
                }
            }

            // Now update the position member to match the updated local position
            position = pos;
            // Similarly, update the fade member to have the latest state
            fade = localFade;
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
} // namespace elem
