#pragma once

#include "Core.h"
#include "../NodeRepr.h"
#include "NodeUtils.h"
#include "Props.h"

namespace elem::lib {
    template<typename T>
    inline constexpr T PI = static_cast<T>(3.14159265358979323846264338327950288);

    // TODO: Maybe we should add an int type to js::Int or something so that we can
    // make things like channel more type safe?

    DEFINE_PROPS_STRUCT(
        IdentityProps,
        key,            std::optional<std::string>,
        channel,        std::optional<js::Number>
    )

    // Unary nodes
    /**
     * TODO: Add this clarification to the docs as well, and maybe js core
     * Identity takes a set of inputs and allows you to choose one to pass through
     * via the `channel` prop. If no inputs are provided, i.e. the node is a leaf
     * node, then the inputs are provided by the host (inputs passed to the root
     * process call)
     */
    static NodeReprSPtr identity(IdentityProps props, std::optional<std::vector<ElemNode>> children=std::nullopt) {
        if (children.has_value()) {
            return NodeRepr::createNode("in", props.takeJsObject(),
                resolve(std::move(*children)));
        }
        return NodeRepr::createNode("in", props.takeJsObject(), {});
    }

    static NodeReprSPtr identity(IdentityProps props, ElemNode x) {
        return NodeRepr::createNode("in", props.takeJsObject(),
            {resolve(std::move(x))});
    }

    static NodeReprSPtr in(IdentityProps props, std::optional<std::vector<ElemNode>> children=std::nullopt) {
        return identity(std::move(props), std::move(children));
    }

    static NodeReprSPtr in(IdentityProps props, ElemNode x) {
        return identity(std::move(props), std::move(x));
    }

    static NodeReprSPtr sin(ElemNode x) {
        return NodeRepr::createNode("sin", {}, {resolve(std::move(x))});
    }

    static NodeReprSPtr cos(ElemNode x) {
        return NodeRepr::createNode("cos", {}, {resolve(std::move(x))});
    }

    static NodeReprSPtr tan(ElemNode x) {
        return NodeRepr::createNode("tan", {}, {resolve(std::move(x))});
    }

    static NodeReprSPtr tanh(ElemNode x) {
        return NodeRepr::createNode("tanh", {}, {resolve(std::move(x))});
    }

    static NodeReprSPtr asinh(ElemNode x) {
        return NodeRepr::createNode("asinh", {}, {resolve(std::move(x))});
    }

    static NodeReprSPtr ln(ElemNode x) {
        return NodeRepr::createNode("ln", {}, {resolve(std::move(x))});
    }

    static NodeReprSPtr log(ElemNode x) {
        return NodeRepr::createNode("log", {}, {resolve(std::move(x))});
    }

    static NodeReprSPtr log2(ElemNode x) {
        return NodeRepr::createNode("log2", {}, {resolve(std::move(x))});
    }

    static NodeReprSPtr ceil(ElemNode x) {
        return NodeRepr::createNode("ceil", {}, {resolve(std::move(x))});
    }

    static NodeReprSPtr floor(ElemNode x) {
        return NodeRepr::createNode("floor", {}, {resolve(std::move(x))});
    }

    static NodeReprSPtr round(ElemNode x) {
        return NodeRepr::createNode("round", {}, {resolve(std::move(x))});
    }

    static NodeReprSPtr sqrt(ElemNode x) {
        return NodeRepr::createNode("sqrt", {}, {resolve(std::move(x))});
    }

    static NodeReprSPtr exp(ElemNode x) {
        return NodeRepr::createNode("exp", {}, {resolve(std::move(x))});
    }

    static NodeReprSPtr abs(ElemNode x) {
        return NodeRepr::createNode("abs", {}, {resolve(std::move(x))});
    }

    // Binary nodes
    static NodeReprSPtr le(ElemNode a, ElemNode b) {
        return NodeRepr::createNode("le", {}, resolve({std::move(a), std::move(b)}));
    }

    static NodeReprSPtr leq(ElemNode a, ElemNode b) {
        return NodeRepr::createNode("leq", {}, resolve({std::move(a), std::move(b)}));
    }

    static NodeReprSPtr ge(ElemNode a, ElemNode b) {
        return NodeRepr::createNode("ge", {}, resolve({std::move(a), std::move(b)}));
    }

    static NodeReprSPtr geq(ElemNode a, ElemNode b) {
        return NodeRepr::createNode("geq", {}, resolve({std::move(a), std::move(b)}));
    }

    static NodeReprSPtr pow(ElemNode a, ElemNode b) {
        return NodeRepr::createNode("pow", {}, resolve({std::move(a), std::move(b)}));
    }

    static NodeReprSPtr eq(ElemNode a, ElemNode b) {
        return NodeRepr::createNode("eq", {}, resolve({std::move(a), std::move(b)}));
    }

    static NodeReprSPtr and_(ElemNode a, ElemNode b) {
        return NodeRepr::createNode("and", {}, resolve({std::move(a), std::move(b)}));
    }

    static NodeReprSPtr or_(ElemNode a, ElemNode b) {
        return NodeRepr::createNode("or", {}, resolve({std::move(a), std::move(b)}));
    }

    // Binary reducing nodes
    static NodeReprSPtr add(std::vector<ElemNode> xs) {
        return NodeRepr::createNode("add", {}, resolve(std::move(xs)));
    }

    static NodeReprSPtr sub(std::vector<ElemNode> xs) {
        return NodeRepr::createNode("sub", {}, resolve(std::move(xs)));
    }

    static NodeReprSPtr mul(std::vector<ElemNode> xs) {
        return NodeRepr::createNode("mul", {}, resolve(std::move(xs)));
    }

    static NodeReprSPtr div(std::vector<ElemNode> xs) {
        return NodeRepr::createNode("div", {}, resolve(std::move(xs)));
    }

    static NodeReprSPtr mod(std::vector<ElemNode> xs) {
        return NodeRepr::createNode("mod", {}, resolve(std::move(xs)));
    }

    static NodeReprSPtr min(std::vector<ElemNode> xs) {
        return NodeRepr::createNode("min", {}, resolve(std::move(xs)));
    }

    static NodeReprSPtr max(std::vector<ElemNode> xs) {
        return NodeRepr::createNode("max", {}, resolve(std::move(xs)));
    }
}