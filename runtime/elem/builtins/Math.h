#pragma once

#include "../GraphNode.h"


namespace elem
{

    template <typename FloatType, FloatType op(FloatType)>
    struct UnaryOperationNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            for (size_t i = 0; i < numSamples; ++i) {
                outputData[i] = op(inputData[0][i]);
            }
        }
    };

    template <typename FloatType, typename BinaryOp>
    struct BinaryOperationNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 2)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // Copy the first input to the output buffer
            for (size_t i = 0; i < numSamples; ++i) {
                outputData[i] = inputData[0][i];
            }

            // Then walk the second channel with the operator
            for (size_t i = 0; i < numSamples; ++i) {
                outputData[i] = op(outputData[i], inputData[1][i]);
            }
        }

        BinaryOp op;
    };

    template <typename FloatType, typename BinaryOp>
    struct BinaryReducingNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // Copy the first input to the output buffer
            for (size_t i = 0; i < numSamples; ++i) {
                outputData[i] = inputData[0][i];
            }

            // Then for each remaining channel, perform the arithmetic operation
            // into the output buffer.
            for (size_t i = 1; i < numChannels; ++i) {
                for (size_t j = 0; j < numSamples; ++j) {
                    outputData[j] = op(outputData[j], inputData[i][j]);
                }
            }
        }

        BinaryOp op;
    };

    template <typename FloatType>
    struct IdentityNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "channel") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                channel.store(static_cast<int>((js::Number) val));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            auto const ch = static_cast<size_t>(channel.load());

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (ch < 0 || ch >= numChannels)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            for (size_t i = 0; i < numSamples; ++i) {
                outputData[i] = inputData[ch][i];
            }
        }

        std::atomic<int> channel = 0;
    };

    template <typename FloatType>
    struct Modulus {
        FloatType operator() (FloatType x, FloatType y) {
            return std::fmod(x, y);
        }
    };

    template <typename FloatType>
    struct SafeDivides {
        FloatType operator() (FloatType x, FloatType y) {
            return (y == FloatType(0)) ? 0 : x / y;
        }
    };

    template <typename FloatType>
    struct Eq {
        FloatType operator() (FloatType x, FloatType y) {
            return std::abs(x - y) <= std::numeric_limits<FloatType>::epsilon();
        }
    };

    template <typename FloatType>
    struct BinaryAnd {
        FloatType operator() (FloatType x, FloatType y) {
            return FloatType(std::abs(FloatType(1) - x) <= std::numeric_limits<FloatType>::epsilon()
                && std::abs(FloatType(1) - y) <= std::numeric_limits<FloatType>::epsilon());
        }
    };

    template <typename FloatType>
    struct BinaryOr {
        FloatType operator() (FloatType x, FloatType y) {
            return FloatType(std::abs(FloatType(1) - x) <= std::numeric_limits<FloatType>::epsilon()
                || std::abs(FloatType(1) - y) <= std::numeric_limits<FloatType>::epsilon());
        }
    };

    template <typename FloatType>
    struct Min {
        FloatType operator() (FloatType x, FloatType y) {
            return std::min(x, y);
        }
    };

    template <typename FloatType>
    struct Max {
        FloatType operator() (FloatType x, FloatType y) {
            return std::max(x, y);
        }
    };

    template <typename FloatType>
    struct SafePow {
        FloatType operator() (FloatType x, FloatType y) {
            // Catch the case of a negative base and a non-integer exponent
            if (x < FloatType(0) && y != std::floor(y))
                return FloatType(0);

            return std::pow(x, y);
        }
    };

} // namespace elem
