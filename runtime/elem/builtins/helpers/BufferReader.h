#pragma once

#include <algorithm>
#include <cassert>
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
        {}

        void engage (double _position) {
            fade.fadeIn();
            position = _position;
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
            std::optional<uint64_t> startOffsetSamples;
            std::optional<uint64_t> stopOffsetSamples;
            bool shouldLoop = false;
            double playbackRate = 1.0;
        };

        template <typename DestType>
        void readAdding(ReadContext<DestType> const& ctx) {
            if (ctx.source == nullptr || fade.fadedOut()) {
                return;
            }

            auto const numChannels = std::min(ctx.numChannels, ctx.source->numChannels());
            auto const bufferSize = ctx.source->numSamples();
            if (numChannels == 0 || bufferSize == 0) {
                return;
            }

            auto const _startOffset = ctx.startOffsetSamples.value_or(0);
            auto const _stopOffset = ctx.stopOffsetSamples.value_or(0);
            auto const startOffset = _startOffset >= 0 ? 
                std::min(_startOffset, static_cast<uint64_t>(bufferSize)) : 0;
            auto const stopOffset = _stopOffset >= 0 ? 
                std::min(_stopOffset, static_cast<uint64_t>(bufferSize)) : 0;
            auto const sampleLength = bufferSize - startOffset - stopOffset;

            elem::GainFade<FloatType> localFade(fade);
            double pos = position;

            for (size_t j = 0; j < numChannels; ++j) {
                pos = position;
                localFade = fade;

                // Here we take a subview of the buffer that ignores samples before the start offset and after the stop offset.
                // This view then gets passed into lerpRead() below. This means we can treat a pos of 0 as `startOffset` and a 
                // pos of 1 as `startOffset + sampleLength`.
                auto bufferView = BufferView<float>::subview(ctx.source->getChannelData(j).data(),
                                                             startOffset, sampleLength);
                for (size_t i = 0; i < ctx.numSamples; ++i) {
                    if (pos >= 1.0) {
                        if (!ctx.shouldLoop) {
                            break;
                        }
                        // Restart the loop. Note there is no crossfade happening yet,
                        // so loops may be discontinuous.
                        pos = pos - 1.0;
                    }
        
                    auto const out = static_cast<DestType>(localFade(lerpRead(bufferView, pos)));
                    ctx.outputData[j][i] += out;

                    pos += (ctx.playbackRate / static_cast<double>(sampleLength));
                }
            }

            // Update the fade member to have the latest state
            fade = localFade;
            position = pos;
        }

        // Linearly interpolates between the two samples adjacent to the given position.
        // @param pos must be a normalized value between 0 and 1.
        static FloatType lerpRead(BufferView<float> const& view, double pos)
        {
            assert(pos >= 0.0 && pos <= 1.0);

            auto* data = view.data();
            auto size = view.size();

            auto const realPos = pos * view.size();
            auto left = static_cast<size_t>(realPos);
            auto right = std::min(left + 1, size - 1);
            auto alpha = realPos - (double) left;

            if (left >= size)
                return FloatType(0);

            if (right >= size)
                return data[left];

            return lerp<FloatType>(static_cast<float>(alpha), data[left], data[right]);
        }

        void reset () {
            fade.reset();
        }

        elem::GainFade<FloatType> fade;

        double position = 0;
    };
} // namespace elem
