#pragma once

#include <AudioFFT.h>

#include <elem/GraphNode.h>
#include <elem/MultiChannelRingBuffer.h>


namespace elem
{

    // An FFT node which reports Float32Array buffers through
    // the event processing interface.
    //
    // Expects exactly one child.
    template <typename FloatType>
    struct FFTNode : public GraphNode<FloatType> {
        FFTNode(NodeId id, FloatType const sr, int const blockSize)
            : GraphNode<FloatType>::GraphNode(id, sr, blockSize)
            , ringBuffer(1)
        {
            // Default size enforced here, this makes sure we have a chance to initialize the fft
            // member object and scratch buffers
            setProperty("size", (js::Number) 1024);
        }

        bool isPowerOfTwo (int x) {
          return (x > 0) && ((x & (x - 1)) == 0);
        }

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "size") {
                if (!val.isNumber())
                    return elem::ReturnCode::InvalidPropertyType();

                int const size = static_cast<int>((js::Number) val);

                if (!isPowerOfTwo(size) || size < 256 || size > 8192)
                    return elem::ReturnCode::InvalidPropertyValue();

                fft.init(size);
                scratchData.resize(size);
                window.resize(size);

                // Hann window
                // for (size_t i = 0; i < size; ++i) {
                //     window[i] = FloatType(0.5 * (1.0 - std::cos(2.0 * M_PI * (i / (double) size))));
                // }

                // Blackman harris
                for (size_t i = 0; i < size; ++i) {
                    FloatType const a0 = 0.35875;
                    FloatType const a1 = 0.48829;
                    FloatType const a2 = 0.14128;
                    FloatType const a3 = 0.01168;

                    FloatType const pi = 3.1415926535897932385;

                    FloatType const t1 = a1 * std::cos(2.0 * pi * (i / (double) (size - 1)));
                    FloatType const t2 = a2 * std::cos(4.0 * pi * (i / (double) (size - 1)));
                    FloatType const t3 = a3 * std::cos(6.0 * pi * (i / (double) (size - 1)));

                    window[i] = a0 - t1 + t2 - t3;
                }
            }

            if (key == "name" && !val.isString())
                return elem::ReturnCode::InvalidPropertyType();

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

            // Copy input to output
            std::copy_n(inputData[0], numSamples, outputData);

            // Fill our ring buffer
            ringBuffer.write(inputData, 1, numSamples);
        }

        void processEvents(std::function<void(std::string const&, js::Value)>& eventHandler) override {
            auto const size = static_cast<size_t>(GraphNode<FloatType>::getPropertyWithDefault("size", js::Number(1024)));

            // If we enough samples, read from the ring buffer into the scratch, then process
            if (ringBuffer.size() >= size)
            {
                std::array<FloatType*, 1> data {scratchData.data()};
                ringBuffer.read(data.data(), 1, size);

                // FFT it
                js::Float32Array re(audiofft::AudioFFT::ComplexSize(size));
                js::Float32Array im(audiofft::AudioFFT::ComplexSize(size));

                // Window the data first...
                for (size_t i = 0; i < size; ++i) {
                  scratchData[i] *= window[i];
                }

                fft.fft(scratchData.data(), re.data(), im.data());

                eventHandler("fft", js::Object({
                    {"source", GraphNode<FloatType>::getPropertyWithDefault("name", js::Value())},
                    {"data", js::Object({
                        {"real", std::move(re)},
                        {"imag", std::move(im)},
                    })}
                }));
            }
        }

        audiofft::AudioFFT fft;
        std::vector<FloatType> window;
        std::vector<FloatType> scratchData;
        MultiChannelRingBuffer<FloatType> ringBuffer;
    };

} // namespace elem
