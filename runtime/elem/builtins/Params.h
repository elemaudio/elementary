#pragma once

#include "../BlockEvents.h"
#include "../GraphNode.h"

namespace elem
{

    // Emits audio rate signals carrying the current value of the parameter
    // identified by the given parameter index.
    template <typename FloatType>
    struct ParameterValueNode : public elem::GraphNode<FloatType> {
        using elem::GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, elem::js::Value const& val) override
        {
            if (key == "index") {
                if (!val.isNumber())
                    return elem::ReturnCode::InvalidPropertyType();

                if (0 > (elem::js::Number) val)
                    return elem::ReturnCode::InvalidPropertyValue();

                index.store(static_cast<size_t>((elem::js::Number) val));
            }

            return elem::GraphNode<FloatType>::setProperty(key, val);
        }

        void process (elem::BlockContext<FloatType> const& ctx) override {
            auto const i = index.load();

            size_t framesProcessed = 0;

            // Process parameter value events from the input events
            ctx.inputEvents.template processEventsOfType<ParamValueEvent>(
                [this, &i, &framesProcessed, &ctx](size_t time, ParamValueEvent const& evt) {
                    if (evt.paramIndex == i) {
                        auto framesRemaining = ctx.numSamples - framesProcessed;
                        std::fill_n(ctx.outputData[0] + framesProcessed, framesRemaining, value);

                        value = evt.value;
                        framesProcessed = time;
                    }
                }
            );

            auto framesRemaining = ctx.numSamples - framesProcessed;
            std::fill_n(ctx.outputData[0] + framesProcessed, framesRemaining, value);
        }

        std::atomic<size_t> index = 0;
        FloatType value = 0;
    };

} // namespace elem
