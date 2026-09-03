#pragma once

#include "Core.h"
#include "Math.h"
#include "NodeUtils.h"

namespace elem::lib {
    static NodeReprSPtr ms2samps(ElemNode t) {
        return mul({sr(), div({std::move(t), 1000.0})});
    }

    static NodeReprSPtr tau2pole(ElemNode t) {
        return exp(div({-1.0, mul({std::move(t), sr()})}));
    }

    static NodeReprSPtr db2gain(ElemNode db) {
        return pow(10.0, mul({std::move(db), 1.0 / 20.0}));
    }

    static NodeReprSPtr select(ElemNode g, ElemNode a, ElemNode b) {
        return add({mul({g, a}), mul({sub({1.0, std::move(g)}), std::move(b)})});
    }

    static NodeReprSPtr gain2db(ElemNode gain) {
        auto isPositive = ge(gain, 0.0);
        return select(
            std::move(isPositive),
            max({
                -120.0,
                mul({
                    20.0,
                    log(std::move(gain))
                })
            }),
            -120.0
        );
    }

    static NodeReprSPtr hann(ElemNode t) {
        return mul({0.5, sub({1.0, cos(mul({2.0 * PI<float>, std::move(t)}))})});
    }
}
