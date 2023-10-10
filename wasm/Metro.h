#pragma once

#include <elem/GraphNode.h>


namespace elem
{

    template <typename FloatType>
    struct MetronomeNode : public GraphNode<FloatType> {
        MetronomeNode(NodeId id, FloatType const sr, int const bs)
            : GraphNode<FloatType>::GraphNode(id, sr, bs)
        {
            // By default the metro interval is exactly one second
            setProperty("interval", js::Value((js::Number) 1000.0));
        }

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "interval") {
                if (!val.isNumber())
                    return elem::ReturnCode::InvalidPropertyType();

                if (0 >= (js::Number) val)
                    return elem::ReturnCode::InvalidPropertyValue();

                // We don't allow an interval smaller than 2 samples because we need at least
                // one sample for the train to go high and one sample for the train to go low.
                // We can't fire a pulse train any faster and have it still be a pulse train.
                auto const is = ((js::Number) val) * 0.001 * GraphNode<FloatType>::getSampleRate();
                intervalSamps.store(static_cast<int64_t>(std::max(2.0, is)));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto* outputData = ctx.outputData;
            auto numSamples = ctx.numSamples;
            auto sampleTime = *static_cast<int64_t*>(ctx.userData);

            auto is = (double) intervalSamps.load();

            for (size_t i = 0; i < numSamples; ++i) {
                auto const t = (double) (sampleTime + i) / is;
                auto const nextOut = FloatType((t - std::floor(t)) < 0.5);

                if (lastOut < FloatType(0.5) && nextOut >= FloatType(0.5)) {
                    eventFlag.store(true);
                }

                outputData[i] = nextOut;
                lastOut = nextOut;
            }
        }

        void processEvents(std::function<void(std::string const&, js::Value)>& eventHandler) override {
            auto flag = eventFlag.exchange(false);

            if (flag) {
                eventHandler("metro", js::Object({
                    {"source", GraphNode<FloatType>::getPropertyWithDefault("name", js::Value())},
                }));
            }
        }

        FloatType lastOut = 0;
        std::atomic<bool> eventFlag = false;
        std::atomic<int64_t> intervalSamps = 0;
        static_assert(std::atomic<int64_t>::is_always_lock_free);
    };

} // namespace elem
