#pragma once

#include <optional>
#include <type_traits>
#include <utility>

#include "../NodeRepr.h"
#include "NodeUtils.h"
#include "Props.h"

namespace elem::lib {

    static NodeReprSPtr sr() {
        return NodeRepr::createNode("sr", {}, {});
    }

    static NodeReprSPtr time() {
        return NodeRepr::createNode("time", {}, {});
    }

    static NodeReprSPtr counter(ElemNode gate) {
        return NodeRepr::createNode("counter", {}, {resolve(std::move(gate))});
    }

    static NodeReprSPtr accum(ElemNode xn, ElemNode reset) {
        return NodeRepr::createNode("accum", {},
            resolve({std::move(xn), std::move(reset)}));
    }

    static NodeReprSPtr phasor(ElemNode rate) {
        return NodeRepr::createNode("phasor", {}, {resolve(std::move(rate))});
    }

    static NodeReprSPtr syncphasor(ElemNode rate, ElemNode reset) {
        return NodeRepr::createNode("sphasor", {}, {resolve(std::move(rate)), resolve(std::move(reset))});
    }

    static NodeReprSPtr latch(ElemNode t, ElemNode x) {
        return NodeRepr::createNode("latch", {},
            resolve({std::move(t), std::move(x)}));
    }

    DEFINE_PROPS_STRUCT(
        MaxHoldProps,
        key,  std::optional<std::string>,
        hold, std::optional<double>
    )

    static NodeReprSPtr maxhold(MaxHoldProps props, ElemNode x, ElemNode reset) {
        return NodeRepr::createNode("maxhold", props.takeJsObject(),
            resolve({std::move(x), std::move(reset)}));
    }

    DEFINE_PROPS_STRUCT(
        OnceProps,
        key, std::optional<std::string>,
        arm, std::optional<bool>
    )

    static NodeReprSPtr once(OnceProps props, ElemNode x) {
        return NodeRepr::createNode("once", props.takeJsObject(),
            {resolve(std::move(x))});
    }

    DEFINE_PROPS_STRUCT(
        RandProps,
        key,  std::optional<std::string>,
        seed, std::optional<js::Number>
    )

    static NodeReprSPtr rand(RandProps props={}) {
        return NodeRepr::createNode("rand", props.takeJsObject(), {});
    }

    DEFINE_PROPS_STRUCT(
        MetroProps,
        key,      std::optional<std::string>,
        name,     std::optional<std::string>,
        interval, std::optional<js::Number>
    )

    static NodeReprSPtr metro(MetroProps props) {
        return NodeRepr::createNode("metro", props.takeJsObject(), {});
    }

    // TODO: Can mode be a variant?
    DEFINE_PROPS_STRUCT(
        SampleProps,
        key,         std::optional<std::string>,
        path,        Required<std::string>,
        mode,        std::optional<std::string>,
        startOffset, std::optional<js::Number>,
        stopOffset,  std::optional<js::Number>
    )

    static NodeReprSPtr sample(SampleProps props, ElemNode trigger, ElemNode rate) {
        return NodeRepr::createNode("sample", props.takeJsObject(),
            resolve({std::move(trigger), std::move(rate)}));
    }

    DEFINE_PROPS_STRUCT(
        TableProps,
        key,        std::optional<std::string>,
        path,       Required<std::string>
    )

    static NodeReprSPtr table(TableProps props, ElemNode t) {
        return NodeRepr::createNode("table", props.takeJsObject(),
            {resolve(std::move(t))});
    }

    DEFINE_PROPS_STRUCT(
        ConvolveProps,
        key,        std::optional<std::string>,
        path,       Required<std::string>
    )

    static NodeReprSPtr convolve(ConvolveProps props, ElemNode x) {
        return NodeRepr::createNode("convolve", props.takeJsObject(),
            {resolve(std::move(x))});
    }

    DEFINE_PROPS_STRUCT(
        SeqProps,
        key,        std::optional<std::string>,
        seq,        Required<js::Array>, // TODO: Strongly type number arrays
        offset,     std::optional<js::Number>,
        hold,       std::optional<bool>,
        loop,       std::optional<bool>
    )

    static NodeReprSPtr seq(SeqProps props, ElemNode trigger, ElemNode reset) {
        return NodeRepr::createNode("seq", props.takeJsObject(),
            resolve({std::move(trigger), std::move(reset)}));
    }

    static NodeReprSPtr seq2(SeqProps props, ElemNode trigger, ElemNode reset) {
        return NodeRepr::createNode("seq2", props.takeJsObject(),
            resolve({std::move(trigger), std::move(reset)}));
    }

    DEFINE_PROPS_STRUCT(
        SparSeqStep,
        value,    Required<js::Number>,
        tickTime, Required<js::Number>
    )

    using SparSeqLoop = std::variant<bool, js::Array>; // TODO: Strongly type number arrays
    DEFINE_PROPS_STRUCT(
        SparSeqProps,
        key,            std::optional<std::string>,
        seq,            Required<std::vector<SparSeqStep>>,
        offset,         std::optional<js::Number>,
        loop,           std::optional<SparSeqLoop>,
        interpolate,    std::optional<js::Number>,
        tickInterval,   std::optional<js::Number>
    );

    static NodeReprSPtr sparseq(SparSeqProps props, ElemNode trigger, ElemNode reset) {
        return NodeRepr::createNode("sparseq", props.takeJsObject(),
            resolve({std::move(trigger), std::move(reset)}));
    }

    DEFINE_PROPS_STRUCT(
        ValueTimeSeqStep,
        value,      Required<js::Number>,
        time,       Required<js::Number>
    )

    DEFINE_PROPS_STRUCT(
        SparSeq2Props,
        key,            std::optional<std::string>,
        seq,            Required<std::vector<ValueTimeSeqStep>>
    )

    static NodeReprSPtr sparseq2(SparSeq2Props props, ElemNode time) {
        return NodeRepr::createNode("sparseq2", props.takeJsObject(),
            {resolve(std::move(time))});
    }

    DEFINE_PROPS_STRUCT(
        SampleSeqProps,
        key,            std::optional<std::string>,
        path,           Required<std::string>,
        seq,            Required<std::vector<ValueTimeSeqStep>>,
        duration,       Required<js::Number>
    )

    static NodeReprSPtr sampleseq(
        SampleSeqProps props,
        ElemNode time
    ) {
        return NodeRepr::createNode("sampleseq", props.takeJsObject(),
            {resolve(std::move(time))});
    }

    DEFINE_PROPS_STRUCT(
        SampleSeq2Props,
        key,            std::optional<std::string>,
        path,           Required<std::string>,
        seq,            Required<std::vector<ValueTimeSeqStep>>,
        duration,       Required<js::Number>,
        stretch,        std::optional<js::Number>,
        shift,          std::optional<js::Number>
    )

    static NodeReprSPtr sampleseq2(
        SampleSeq2Props props,
        ElemNode time
    ) {
        return NodeRepr::createNode("sampleseq2", props.takeJsObject(),
            {resolve(std::move(time))});
    }

    static NodeReprSPtr pole(ElemNode p, ElemNode x) {
        return NodeRepr::createNode("pole", {},
            resolve({std::move(p), std::move(x)}));
    }

    static NodeReprSPtr env(ElemNode atkPole, ElemNode relPole, ElemNode x) {
        return NodeRepr::createNode("env", {},
            resolve({std::move(atkPole), std::move(relPole), std::move(x)}));
    }

    static NodeReprSPtr z(ElemNode x) {
        return NodeRepr::createNode("z", {}, {resolve(std::move(x))});
    }

    DEFINE_PROPS_STRUCT(
        DelayProps,
        key,        std::optional<std::string>,
        size,       Required<js::Number>
    )

    static NodeReprSPtr delay(DelayProps props, ElemNode len, ElemNode fb, ElemNode x) {
        return NodeRepr::createNode("delay", props.takeJsObject(),
            resolve({std::move(len), std::move(fb), std::move(x)}));
    }

    DEFINE_PROPS_STRUCT(
        SDelayProps,
        key,        std::optional<std::string>,
        size,       Required<js::Number>
    )

    static NodeReprSPtr sdelay(SDelayProps props, ElemNode x) {
        return NodeRepr::createNode("sdelay", props.takeJsObject(),
            {resolve(std::move(x))});
    }

    static NodeReprSPtr prewarp(ElemNode fc) {
        return NodeRepr::createNode("prewarp", {}, {resolve(std::move(fc))});
    }

    // TODO: Maybe we could make mode into an enum or variant to enumerate its possible values

    DEFINE_PROPS_STRUCT(
        MM1PProps,
        key,        std::optional<std::string>,
        mode,       std::optional<std::string>
    )

    static NodeReprSPtr mm1p(MM1PProps props, ElemNode fc, ElemNode x) {
        return NodeRepr::createNode("mm1p", props.takeJsObject(),
            resolve({std::move(fc), std::move(x)}));
    }

    DEFINE_PROPS_STRUCT(
        SVFProps,
        key,        std::optional<std::string>,
        mode,       std::optional<std::string>
    )

    static NodeReprSPtr svf(SVFProps props, ElemNode fc, ElemNode q, ElemNode x) {
        return NodeRepr::createNode("svf", props.takeJsObject(),
            resolve({std::move(fc), std::move(q), std::move(x)}));
    }

    DEFINE_PROPS_STRUCT(
        SVFShelfProps,
        key,        std::optional<std::string>,
        mode,       std::optional<std::string>
    )

    static NodeReprSPtr svfshelf(SVFShelfProps props, ElemNode fc, ElemNode q, ElemNode gainDecibels, ElemNode x) {
        return NodeRepr::createNode("svfshelf", props.takeJsObject(),
            resolve({std::move(fc), std::move(q), std::move(gainDecibels), std::move(x)}));
    }

    static NodeReprSPtr biquad(
        ElemNode b0,
        ElemNode b1,
        ElemNode b2,
        ElemNode a1,
        ElemNode a2,
        ElemNode x
    ) {
        return NodeRepr::createNode("biquad", {},
                                         resolve({
                                             std::move(b0), std::move(b1), std::move(b2),
                                             std::move(a1), std::move(a2), std::move(x)
                                         }));
    }

    DEFINE_PROPS_STRUCT(
        TapProps,
        key,        std::optional<std::string>,
        name,       Required<std::string>
    )

    static NodeReprSPtr tapIn(TapProps props) {
        return NodeRepr::createNode("tapIn", props.takeJsObject(), {});
    }

    static NodeReprSPtr tapOut(TapProps props, ElemNode x) {
        return NodeRepr::createNode("tapOut", props.takeJsObject(), {resolve(std::move(x))});
    }

    DEFINE_PROPS_STRUCT(
        MeterProps,
        key,        std::optional<std::string>,
        name,       std::optional<std::string>
    )

    static NodeReprSPtr meter(MeterProps props, ElemNode x) {
        return NodeRepr::createNode("meter", props.takeJsObject(),
            {resolve(std::move(x))});
    }

    DEFINE_PROPS_STRUCT(
        SnapshotProps,
        key,        std::optional<std::string>,
        name,       std::optional<std::string>
    )

    static NodeReprSPtr snapshot(SnapshotProps props, ElemNode trigger, ElemNode x) {
        return NodeRepr::createNode("snapshot", props.takeJsObject(),
            resolve({std::move(trigger), std::move(x)}));
    }

    DEFINE_PROPS_STRUCT(
        ScopeProps,
        key,        std::optional<std::string>,
        name,       std::optional<std::string>,
        size,       std::optional<js::Number>,
        channels,   std::optional<js::Number>
    )

    static NodeReprSPtr scope(ScopeProps props, std::vector<ElemNode> children) {
        return NodeRepr::createNode("scope", props.takeJsObject(),
            resolve(std::move(children)));
    }

    DEFINE_PROPS_STRUCT(
        FFTProps,
        key,        std::optional<std::string>,
        name,       std::optional<std::string>,
        size,       std::optional<js::Number>
    )

    static NodeReprSPtr fft(FFTProps props, ElemNode x) {
        return NodeRepr::createNode("fft", props.takeJsObject(),
            {resolve(std::move(x))});
    }

    DEFINE_PROPS_STRUCT(
        CaptureProps,
        key,        std::optional<std::string>
    )

    static NodeReprSPtr capture(CaptureProps props, ElemNode g, ElemNode x) {
        return NodeRepr::createNode("capture", props.takeJsObject(),
            resolve({std::move(g), std::move(x)}));
    }
}
