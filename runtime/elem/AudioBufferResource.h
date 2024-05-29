#pragma once

#include <vector>
#include "SharedResource.h"
#include "Types.h"


namespace elem
{

    class AudioBufferResource : public SharedResource {
    public:
        AudioBufferResource(float* data, size_t numSamples)
        {
            channels.push_back(std::vector<float>(data, data + numSamples));
        }

        AudioBufferResource(float** data, size_t numChannels, size_t numSamples)
        {
            for (size_t i = 0; i < numChannels; ++i) {
                channels.push_back(std::vector<float>(data[i], data[i] + numSamples));
            }
        }

        AudioBufferResource(size_t numChannels, size_t numSamples)
        {
            for (size_t i = 0; i < numChannels; ++i) {
                channels.push_back(std::vector<float>(numSamples));
            }
        }

        BufferView<float> getChannelData(size_t channelIndex) override
        {
            if (channelIndex < channels.size()) {
                auto& chan = channels[channelIndex];
                return BufferView<float>(chan.data(), chan.size());
            }

            return BufferView<float>(nullptr, 0);
        }

        size_t numChannels() override {
            return channels.size();
        }

        size_t numSamples() override {
            if (numChannels() > 0) {
                return channels[0].size();
            }

            return 0;
        }

    private:
        // TODO: make this one contiguous chunk of data
        std::vector<std::vector<float>> channels;
    };

} // namespace elem
