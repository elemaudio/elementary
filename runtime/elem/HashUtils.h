#pragma once

#include "JSON.h"
#include "Types.h"

/**
 * Structural node hashes in the elementary audio graph is done using the FNV-1a non-cryptographic hashing algorithm.
 */
namespace elem::HashUtils {
    static constexpr NodeId kFnvOffsetBasis = 0x811c9dc5;

    static NodeId mixNumber(const NodeId seed, const NodeId n) {
        return (seed ^ n) * 0x01000193;
    }

    static NodeId hashString(const NodeId seed, std::string const &s) {
        NodeId r = seed;

        for (char c: s) {
            r = mixNumber(r, c);
        }

        return r;
    }

    static NodeId hashProps(NodeId seed, js::Object const &props) {
        if (auto const it = props.find("key"); it != props.end() && it->second.isString()) {
            return hashString(seed, it->second);
        }

        return hashString(seed, js::serialize(js::Value(props)));
    }

    static NodeId finalizeHash(const NodeId n) {
        return n & 0x7fffffff;
    }

    static NodeId hashNode(std::string const &kind, const js::Object &props, const std::vector<NodeId> &children) {
        NodeId r = hashString(kFnvOffsetBasis, kind);
        r = hashProps(r, props);
        for (const auto child: children) {
            r = mixNumber(r, child);
        }
        // TODO: Why do we use signed int but ensure it's always positive with finalHash?
        return finalizeHash(r);
    }
}

