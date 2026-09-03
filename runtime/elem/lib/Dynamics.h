#pragma once

#include "Core.h"
#include "Math.h"
#include "NodeUtils.h"
#include "Signals.h"

namespace elem::lib {
    /**
     * A simple hard knee compressor with parameterized attack and release times,
     * threshold, and compression ratio.
     */
    static NodeReprSPtr compress(
        ElemNode attackMs,
        ElemNode releaseMs,
        ElemNode threshold,
        ElemNode ratio,
        ElemNode sidechain,
        ElemNode xn
    ) {
        auto envNode = env(
            tau2pole(mul({0.001, std::move(attackMs)})),
            tau2pole(mul({0.001, std::move(releaseMs)})),
            std::move(sidechain)
        );

        auto envDecibels = gain2db(std::move(envNode));

        // Calculate gain multiplier from the ratio (1 - 1/ratio)
        auto adjustedRatio = sub({1.0, div({1.0, std::move(ratio)})});

        // Calculate gain reduction in dB
        auto gain = mul({std::move(adjustedRatio), sub({std::move(threshold), std::move(envDecibels)})});

        // Ensuring gain is not positive
        auto cleanGain = min({0.0, std::move(gain)});

        // Convert the gain reduction in dB to a gain factor
        auto compressedGain = db2gain(std::move(cleanGain));

        return mul({std::move(xn), std::move(compressedGain)});
    }

    /**
     * A simple softknee compressor with parameterized attack and release times,
     * threshold, compression ratio and knee width.
     *
     * Functions like regular compress when kneeWidth is 0.
     */
    static NodeReprSPtr skcompress(
        ElemNode attackMs,
        ElemNode releaseMs,
        ElemNode threshold,
        ElemNode ratio,
        ElemNode kneeWidth,
        ElemNode sidechain,
        ElemNode xn
    ) {
        auto envNode = env(
            tau2pole(mul({0.001, std::move(attackMs)})),
            tau2pole(mul({0.001, std::move(releaseMs)})),
            std::move(sidechain)
        );

        auto envDecibels = gain2db(std::move(envNode));

        // Calculate the soft knee bounds around the threshold
        auto lowerKneeBound = sub({threshold, div({kneeWidth, 2.0})});
        auto upperKneeBound = add({threshold, div({kneeWidth, 2.0})});

        // Check if the envelope is in the soft knee range
        auto isInSoftKneeRange = and_(
            geq(envDecibels, lowerKneeBound),
            leq(envDecibels, std::move(upperKneeBound))
        );

        // Calculate gain multiplier from the ratio (1 - 1/ratio)
        auto adjustedRatio = sub({1.0, div({1.0, std::move(ratio)})});

        // Gain calculation
        // When in soft knee range, do:
        //   0.5 * adjustedRatio * ((envDecibels - lowerKneeBound) / kneeWidth) * (lowerKneeBound - envDecibels)
        // Else do:
        //   adjustedRatio * (threshold - envDecibels)
        auto gain = select(
            std::move(isInSoftKneeRange),
            mul({
                div({adjustedRatio, 2.0}),
                mul({
                    div({sub({envDecibels, lowerKneeBound}), std::move(kneeWidth)}),
                    sub({std::move(lowerKneeBound), envDecibels})
                })
            }),
            mul({adjustedRatio, sub({std::move(threshold), envDecibels})})
        );

        // Ensuring gain is not positive
        auto cleanGain = min({0.0, std::move(gain)});

        // Convert the gain reduction in dB to a gain factor
        auto compressedGain = db2gain(std::move(cleanGain));

        return mul({std::move(xn), std::move(compressedGain)});
    }
}
