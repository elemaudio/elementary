#pragma once

#include "builtins/Analyzers.h"
#include "builtins/Core.h"
#include "builtins/Delays.h"
#include "builtins/Feedback.h"
#include "builtins/Filters.h"
#include "builtins/Math.h"
#include "builtins/Metro.h"
#include "builtins/Noise.h"
#include "builtins/Sample.h"
#include "builtins/Seq2.h"
#include "builtins/SparSeq.h"
#include "builtins/Table.h"


namespace elem
{

    namespace detail
    {
        /** A quick helper factory for default node types. */
        template <typename NodeType>
        struct GenericNodeFactory
        {
            template <typename NodeIdType>
            auto operator() (NodeIdType const id, typename NodeType::ValueType fs, int const blockSize) {
                return std::make_shared<NodeType>(id, fs, blockSize);
            }
        };
    }

    template <typename FloatType>
    struct DefaultNodeTypes {

        template <typename Fn>
        static void forEach(Fn&& callback) {
            using namespace detail;

            // Unary math nodes
            callback("in",        GenericNodeFactory<IdentityNode<FloatType>>());
            callback("sin",       GenericNodeFactory<UnaryOperationNode<FloatType, std::sin>>());
            callback("cos",       GenericNodeFactory<UnaryOperationNode<FloatType, std::cos>>());
            callback("tan",       GenericNodeFactory<UnaryOperationNode<FloatType, std::tan>>());
            callback("tanh",      GenericNodeFactory<UnaryOperationNode<FloatType, std::tanh>>());
            callback("asinh",     GenericNodeFactory<UnaryOperationNode<FloatType, std::asinh>>());
            callback("ln",        GenericNodeFactory<UnaryOperationNode<FloatType, std::log>>());
            callback("log",       GenericNodeFactory<UnaryOperationNode<FloatType, std::log10>>());
            callback("log2",      GenericNodeFactory<UnaryOperationNode<FloatType, std::log2>>());
            callback("ceil",      GenericNodeFactory<UnaryOperationNode<FloatType, std::ceil>>());
            callback("floor",     GenericNodeFactory<UnaryOperationNode<FloatType, std::floor>>());
            callback("sqrt",      GenericNodeFactory<UnaryOperationNode<FloatType, std::sqrt>>());
            callback("exp",       GenericNodeFactory<UnaryOperationNode<FloatType, std::exp>>());
            callback("abs",       GenericNodeFactory<UnaryOperationNode<FloatType, std::abs>>());

            // Binary math nodes
            callback("le",        GenericNodeFactory<BinaryOperationNode<FloatType, std::less<FloatType>>>());
            callback("leq",       GenericNodeFactory<BinaryOperationNode<FloatType, std::less_equal<FloatType>>>());
            callback("ge",        GenericNodeFactory<BinaryOperationNode<FloatType, std::greater<FloatType>>>());
            callback("geq",       GenericNodeFactory<BinaryOperationNode<FloatType, std::greater_equal<FloatType>>>());
            callback("pow",       GenericNodeFactory<BinaryOperationNode<FloatType, SafePow<FloatType>>>());
            callback("eq",        GenericNodeFactory<BinaryOperationNode<FloatType, Eq<FloatType>>>());
            callback("and",       GenericNodeFactory<BinaryOperationNode<FloatType, BinaryAnd<FloatType>>>());
            callback("or",        GenericNodeFactory<BinaryOperationNode<FloatType, BinaryOr<FloatType>>>());

            // Reducing nodes
            callback("add",       GenericNodeFactory<BinaryReducingNode<FloatType, std::plus<FloatType>>>());
            callback("sub",       GenericNodeFactory<BinaryReducingNode<FloatType, std::minus<FloatType>>>());
            callback("mul",       GenericNodeFactory<BinaryReducingNode<FloatType, std::multiplies<FloatType>>>());
            callback("div",       GenericNodeFactory<BinaryReducingNode<FloatType, SafeDivides<FloatType>>>());
            callback("mod",       GenericNodeFactory<BinaryReducingNode<FloatType, Modulus<FloatType>>>());
            callback("min",       GenericNodeFactory<BinaryReducingNode<FloatType, Min<FloatType>>>());
            callback("max",       GenericNodeFactory<BinaryReducingNode<FloatType, Max<FloatType>>>());

            // Core nodes
            callback("root",      GenericNodeFactory<RootNode<FloatType>>());
            callback("const",     GenericNodeFactory<ConstNode<FloatType>>());
            callback("phasor",    GenericNodeFactory<PhasorNode<FloatType>>());
            callback("sr",        GenericNodeFactory<SampleRateNode<FloatType>>());
            callback("time",      GenericNodeFactory<SampleTimeNode<FloatType>>());
            callback("seq",       GenericNodeFactory<SequenceNode<FloatType>>());
            callback("seq2",      GenericNodeFactory<Seq2Node<FloatType>>());
            callback("sparseq",   GenericNodeFactory<SparSeqNode<FloatType>>());
            callback("counter",   GenericNodeFactory<CounterNode<FloatType>>());
            callback("accum",     GenericNodeFactory<AccumNode<FloatType>>());
            callback("latch",     GenericNodeFactory<LatchNode<FloatType>>());
            callback("maxhold",   GenericNodeFactory<MaxHold<FloatType>>());
            callback("once",      GenericNodeFactory<OnceNode<FloatType>>());
            callback("rand",      GenericNodeFactory<UniformRandomNoiseNode<FloatType>>());
            callback("metro",     GenericNodeFactory<MetronomeNode<FloatType>>());

            // Delay nodes
            callback("delay",     GenericNodeFactory<VariableDelayNode<FloatType>>());
            callback("sdelay",    GenericNodeFactory<SampleDelayNode<FloatType>>());
            callback("z",         GenericNodeFactory<SingleSampleDelayNode<FloatType>>());

            // Filter nodes
            callback("pole",      GenericNodeFactory<OnePoleNode<FloatType>>());
            callback("env",       GenericNodeFactory<EnvelopeNode<FloatType>>());
            callback("biquad",    GenericNodeFactory<BiquadFilterNode<FloatType>>());

            // Feedback nodes
            callback("tapIn",     GenericNodeFactory<TapInNode<FloatType>>());
            callback("tapOut",    GenericNodeFactory<TapOutNode<FloatType>>());

            // Sample/Buffer nodes
            callback("sample",    GenericNodeFactory<SampleNode<FloatType>>());
            callback("table",     GenericNodeFactory<TableNode<FloatType>>());

            // Analyzer nodes
            callback("meter",     GenericNodeFactory<MeterNode<FloatType>>());
            callback("scope",     GenericNodeFactory<ScopeNode<FloatType>>());
            callback("snapshot",  GenericNodeFactory<SnapshotNode<FloatType>>());
        }
    };

} // namespace elem
