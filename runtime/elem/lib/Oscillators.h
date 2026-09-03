#pragma once

#include "Core.h"
#include "Math.h"
#include "NodeUtils.h"

namespace elem::lib {
    static NodeReprSPtr train(ElemNode rate) {
        return le(phasor(std::move(rate)), 0.5);
    }

    static NodeReprSPtr cycle(ElemNode rate) {
        return sin(mul({resolve(2.0 * PI<float>), phasor(resolve(std::move(rate)))}));
    }

    static NodeReprSPtr saw(ElemNode rate) {
        return sub({mul({2.0, phasor(std::move(rate))}), 1.0});
    }

    static NodeReprSPtr square(ElemNode rate) {
        return sub({mul({2.0, train(std::move(rate))}), 1.0});
    }

    static NodeReprSPtr triangle(ElemNode rate) {
        return mul({2.0, sub({0.5, abs(saw(std::move(rate)))})});
    }

    static NodeReprSPtr blepsaw(ElemNode rate) {
        return NodeRepr::createNode("blepsaw", {}, {resolve(std::move(rate))});
    }

    static NodeReprSPtr blepsquare(ElemNode rate) {
        return NodeRepr::createNode("blepsquare", {}, {resolve(std::move(rate))});
    }

    static NodeReprSPtr bleptriangle(ElemNode rate) {
        return NodeRepr::createNode("bleptriangle", {}, {resolve(std::move(rate))});
    }

    static NodeReprSPtr noise(RandProps props = {}) {
        return sub({mul({2.0, rand(std::move(props))}), 1.0});
    }
}