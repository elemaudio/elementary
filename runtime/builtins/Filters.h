#pragma once

#include "../GraphNode.h"


namespace elem
{

    // A very simple one pole filter.
    //
    // Receives the pole position, p, as a signal, and the input to filter as the second signal.
    // See: https://ccrma.stanford.edu/~jos/filters/One_Pole.html
    template <typename FloatType>
    struct OnePoleNode : public GraphNode<FloatType> {
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

            // First channel is the pole position, second channel is the input signal
            for (size_t i = 0; i < numSamples; ++i) {
                auto const p = inputData[0][i];
                auto const x = inputData[1][i];

                z = x + p * z;
                outputData[i] = z;
            }
        }

        FloatType z = 0;
    };

    // A unity-gain one pole filter which takes a different coefficient depending on the
    // amplitude of the incoming signal.
    //
    // This is an envelope follower with a parameterizable attack and release
    // coefficient, given by the first and second input channel respectively.
    template <typename FloatType>
    struct EnvelopeNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            if (numChannels < 3)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // First channel is the pole position when the input exceeds the current
            // filter output, second channel is the pole position otherwise. The third
            // channel is the input signal to be filtered
            for (size_t i = 0; i < numSamples; ++i) {
                auto const ap = inputData[0][i];
                auto const rp = inputData[1][i];
                auto const vn = std::abs(inputData[2][i]);

                if (std::abs(vn) > z) {
                    z = ap * (z - vn) + vn;
                } else {
                    z = rp * (z - vn) + vn;
                }

                outputData[i] = z;
            }
        }

        FloatType z = 0;
    };

    // The classic second order Transposed Direct Form II filter implementation.
    //
    // Receives each coefficient as a signal, with the signal to filter as the last child.
    // See:
    //  https://ccrma.stanford.edu/~jos/filters/Transposed_Direct_Forms.html
    //  https://os.mbed.com/users/simon/code/dsp/docs/tip/group__BiquadCascadeDF2T.html
    template <typename FloatType>
    struct BiquadFilterNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 6)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            for (size_t i = 0; i < numSamples; ++i) {
                auto const b0 = inputData[0][i];
                auto const b1 = inputData[1][i];
                auto const b2 = inputData[2][i];
                auto const a1 = inputData[3][i];
                auto const a2 = inputData[4][i];
                auto const x = inputData[5][i];

                auto const y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;

                outputData[i] = y;
            }
        }

        FloatType z1 = 0;
        FloatType z2 = 0;
    };

} // namespace elem

// For later....
// This is my gendsp patch for remnant's tpt svf. Rewrite it as a filter node here.
/*
// Prewarp the cutoff frequency according to the
// bilinear transform.
prewarp_bt(fc) {
	wd = 2.0 * pi * fc;
	T = 1.0 / samplerate;
	wa = (2.0 / T) * tan(wd * T / 2.0);

	return wa;
}

// Implements Vadim Zavalishin's TPT 2-pole SVF, following Section 4
// from his book v2.0.0-alpha (page 110-111), and adding a cheap
// nonlinearity in the integrator.
//
// TODO: A nice improvement would be to properly address the
// nonlinearity in the feedback path and implement a root solver for
// a more accurate approximation.
// 
// Params:
//  `xn`: Input sample
//	`wa`: Analog cutoff requency; result of the cutoff prewarping
//	`R`: Filter resonance
svf_nl(xn, wa, R) {
	// State registers
	History z1(0.);
	History z2(0.);
	
	// Integrator gain coefficient
	T = 1.0 / samplerate;
	g = wa * T / 2.0;
	
	// Filter taps
	hp = (xn - (2.0 * R + g) * z1 - z2) / (1.0 + 2.0 * R * g + g * g);
	v1 = g * hp;
	bp = v1 + z1;
	v2 = g * bp;
	lp = v2 + z2;
	
	// Update state
	z1 = asinh(bp + v1);
	z2 = asinh(lp + v2);
	
	// We form the normalized, "classic" bandpass here.
	bpNorm = bp * 2.0 * R;
	
	return lp, bpNorm, hp;
}

// Tick the filter
lp, bp, hp = svf_nl(in1, prewarp_bt(in2), in3);

out1 = lp;
out2 = bp;
out3 = hp;
*/
