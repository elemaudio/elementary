#pragma once

#include <algorithm>
#include <cstddef>

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
        void readAdding(SharedResource* resource, DestType** outputData, size_t numChannels, size_t numSamples) {
            elem::GainFade<FloatType> localFade(fade);

            for (size_t j = 0; j < std::min(numChannels, resource->numChannels()); ++j) {
                auto bufferView = resource->getChannelData(j);
                auto bufferSize = bufferView.size();
                auto* sourceData = bufferView.data();

                // Reinitialize the local copy to match our member instance
                localFade = fade;

                for (size_t i = 0; (i < numSamples) && ((position + i) < bufferSize); ++i) {
                    outputData[j][i] += static_cast<DestType>(localFade(sourceData[position + i]));
                }
            }

            // Here we have a localFade instance that has finished running over a block, which
            // represents where our class instance should now be
            fade = localFade;

            // And update our position
            position += numSamples;
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