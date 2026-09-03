#pragma once

#include "Core.h"
#include "Math.h"
#include "NodeUtils.h"
#include "Signals.h"
#include "Filters.h"

namespace elem::lib {
    /**
     * An exponential ADSR envelope generator, triggered by the gate signal, g.
     *
     * When the gate is high (1), this generates the ADS phase. When the gate is
     * low (0), the R phase.
     */
    static NodeReprSPtr adsr(
        ElemNode attackSec,
        ElemNode decaySec,
        ElemNode sustain,
        ElemNode releaseSec,
        ElemNode gate
    ) {
        auto atkSamps = mul({attackSec, sr()});
        auto atkGate = le(counter(gate), std::move(atkSamps));

        auto targetValue = select(gate, select(atkGate, 1.0, std::move(sustain)), 0.0);

        // Clamp the values to a minimum of 0.1ms because a time constant of 0 yields
        // a divide-by-zero in the pole calculation
        auto t60 = max({0.0001, select(std::move(gate), select(std::move(atkGate), std::move(attackSec), std::move(decaySec)), std::move(releaseSec))});

        // Accelerate the phase time when calculating the pole position to ensure
        // we reach closer to the target value before moving to the next phase.
        //
        // See: https://ccrma.stanford.edu/~jos/mdft/Audio_Decay_Time_T60.html
        auto p = tau2pole(div({std::move(t60), 6.91}));

        return smooth(std::move(p), std::move(targetValue));
    }
}
