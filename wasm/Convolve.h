#pragma once

#include <TwoStageFFTConvolver.h>

#include <elem/GraphNode.h>
#include <elem/SingleWriterSingleReaderQueue.h>


namespace elem
{

    namespace detail
    {
        template <typename FromType, typename ToType>
        void copy_cast_n(FromType const* input, size_t numSamples, ToType* output)
        {
            for (size_t i = 0; i < numSamples; ++i) {
                output[i] = static_cast<ToType>(input[i]);
            }
        }
    }

    template <typename FloatType>
    struct ConvolutionNode : public GraphNode<FloatType> {
        ConvolutionNode(NodeId id, FloatType const sr, int const blockSize)
            : GraphNode<FloatType>::GraphNode(id, sr, blockSize)
        {
            if constexpr (std::is_same_v<FloatType, double>) {
                scratchIn.resize(blockSize);
                scratchOut.resize(blockSize);
            }
        }

        int setProperty(std::string const& key, js::Value const& val, SharedResourceMap<FloatType>& resources) override
        {
            if (key == "path") {
                if (!val.isString())
                    return elem::ReturnCode::InvalidPropertyType();

                if (!resources.has((js::String) val))
                    return elem::ReturnCode::InvalidPropertyValue();

                auto ref = resources.get((js::String) val);
                auto co = std::make_shared<fftconvolver::TwoStageFFTConvolver>();

                co->reset();

                // TODO!!!
                // This can just be re-enabled by default once we get to the new shared
                // resource format
                // co->init(512, 4096, ref->data(), ref->size());

                convolverQueue.push(std::move(co));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // First order of business: grab the most recent convolver to use if
            // there's anything in the queue. This behavior means that changing the convolver
            // impulse response while playing will cause a discontinuity.
            while (convolverQueue.size() > 0)
                convolverQueue.pop(convolver);

            if (numChannels == 0 || convolver == nullptr)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            if constexpr (std::is_same_v<FloatType, float>) {
                convolver->process(inputData[0], outputData, numSamples);
            }

            if constexpr (std::is_same_v<FloatType, double>) {
                auto* scratchDataIn = scratchIn.data();
                auto* scratchDataOut = scratchOut.data();

                detail::copy_cast_n<double, float>(inputData[0], numSamples, scratchDataIn);
                convolver->process(scratchDataIn, scratchDataOut, numSamples);
                detail::copy_cast_n<float, double>(scratchDataOut, numSamples, outputData);
            }
        }

        SingleWriterSingleReaderQueue<std::shared_ptr<fftconvolver::TwoStageFFTConvolver>> convolverQueue;
        std::shared_ptr<fftconvolver::TwoStageFFTConvolver> convolver;

        std::vector<float> scratchIn;
        std::vector<float> scratchOut;
    };

} // namespace elem
