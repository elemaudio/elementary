#pragma once

#include "Core.h"
#include "Math.h"
#include "NodeUtils.h"
#include "Signals.h"

namespace elem::lib {
    static NodeReprSPtr smooth(ElemNode p, ElemNode x) {
        auto onePole = sub({1.0, p});
        return pole(std::move(p), mul({std::move(onePole), std::move(x)}));
    }

    static NodeReprSPtr sm(ElemNode x) {
        return smooth(tau2pole(0.02), std::move(x));
    }

    static NodeReprSPtr zero(ElemNode b0, ElemNode b1, ElemNode x) {
        return sub({mul({std::move(b0), x}), mul({std::move(b1), z(std::move(x))})});
    }

    static NodeReprSPtr dcblock(ElemNode x) {
        return pole(0.995, zero(1.0, 1.0, std::move(x)));
    }

    static NodeReprSPtr df11(ElemNode b0, ElemNode b1, ElemNode a1, ElemNode x) {
        return pole(std::move(a1), zero(std::move(b0), std::move(b1), std::move(x)));
    }

    static NodeReprSPtr lowpass(ElemNode fc, ElemNode q, ElemNode x) {
        return svf({{}, "lowpass"}, std::move(fc), std::move(q), std::move(x));
    }

    static NodeReprSPtr highpass(ElemNode fc, ElemNode q, ElemNode x) {
        return svf({{}, "highpass"}, std::move(fc), std::move(q), std::move(x));
    }

    static NodeReprSPtr bandpass(ElemNode fc, ElemNode q, ElemNode x) {
        return svf({{}, "bandpass"}, std::move(fc), std::move(q), std::move(x));
    }

    static NodeReprSPtr notch(ElemNode fc, ElemNode q, ElemNode x) {
        return svf({{}, "notch"}, std::move(fc), std::move(q), std::move(x));
    }

    static NodeReprSPtr allpass(ElemNode fc, ElemNode q, ElemNode x) {
        return svf({{}, "allpass"}, std::move(fc), std::move(q), std::move(x));
    }

    static NodeReprSPtr peak(ElemNode fc, ElemNode q, ElemNode gainDecibels, ElemNode x) {
        return svfshelf({{}, "peak"}, std::move(fc), std::move(q), std::move(gainDecibels), std::move(x));
    }

    static NodeReprSPtr lowshelf(ElemNode fc, ElemNode q, ElemNode gainDecibels, ElemNode x) {
        return svfshelf({{}, "lowshelf"}, std::move(fc), std::move(q), std::move(gainDecibels), std::move(x));
    }

    static NodeReprSPtr highshelf(ElemNode fc, ElemNode q, ElemNode gainDecibels, ElemNode x) {
        return svfshelf({{}, "highshelf"}, std::move(fc), std::move(q), std::move(gainDecibels), std::move(x));
    }

    static NodeReprSPtr pink(ElemNode x) {
        auto clip = [](ElemNode lo, ElemNode hi, ElemNode v) {
            return min({std::move(hi), max({std::move(lo), std::move(v)})});
        };

        return clip(-1.0, 1.0,
            mul({
                db2gain(-30.0),
                add({
                    pole(0.99765, mul({x, 0.099046})),
                    pole(0.963, mul({x, 0.2965164})),
                    pole(0.57, mul({x, 1.0526913})),
                    mul({0.1848, std::move(x)}),
                })
            }));
    }
}
