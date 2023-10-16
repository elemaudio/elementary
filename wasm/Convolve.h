#pragma once

#include <TwoStageFFTConvolver.h>

#include <elem/GraphNode.h>
#include <elem/SingleWriterSingleReaderQueue.h>


namespace elem
{

    template <typename FloatType>
    struct ConvolutionNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

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
                co->init(512, 4096, ref->data(), ref->size());

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

            convolver->process(inputData[0], outputData, numSamples);
        }

        SingleWriterSingleReaderQueue<std::shared_ptr<fftconvolver::TwoStageFFTConvolver>> convolverQueue;
        std::shared_ptr<fftconvolver::TwoStageFFTConvolver> convolver;
    };

} // namespace elem
