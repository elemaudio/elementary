#pragma once

#include <fstream>
#include <sstream>
#include <string>

#include "SnapshotTestUtils.h"
#include "elem/deps/json.hpp"

namespace elem::test {
    inline std::string graphMermaidFilePath(std::string const &name) {
        return std::string(NATIVE_RENDERER_TESTS_DIR) + "/snapshots/" + name + ".snapshot.md";
    }

    // Mermaid node labels are double-quoted strings, so any embedded quote must be
    // escaped to avoid breaking out of the label.
    inline std::string escapeMermaidLabel(std::string const &text) {
        std::string escaped;
        for (char const c : text) {
            if (c == '"') {
                escaped += "#quot;";
            } else {
                escaped += c;
            }
        }
        return escaped;
    }

    // Renders a graph snapshot (as produced by Runtime::snapshot()) as a Mermaid
    // flowchart: one node per hash id/kind pair with its props, one edge per
    // parent->child inlet connection, labeled with the output channel the child
    // addresses.
    inline std::string graphMermaidDiagram(nlohmann::json const &graph) {
        std::ostringstream out;
        out << "```mermaid\n";
        out << "flowchart TD\n";

        for (auto const &[nodeId, node] : graph.items()) {
            auto const kind = node.value("kind", "");
            auto const props = node.value("props", nlohmann::json::object());

            out << "    n" << nodeId << "[\"" << nodeId << "<br/>" << kind;
            for (auto const &[propKey, propValue] : props.items()) {
                out << "<br/>" << escapeMermaidLabel(propKey) << "=" << escapeMermaidLabel(propValue.dump());
            }
            out << "\"]\n";
        }

        for (auto const &[nodeId, node] : graph.items()) {
            for (auto const &inlet : node.value("inlets", nlohmann::json::array())) {
                auto const source = inlet.value("source", std::string());
                auto const channel = static_cast<long long>(inlet.value("outletChannel", 0.0));
                out << "    n" << nodeId << " -->|ch " << channel << "| n" << source << "\n";
            }
        }

        out << "```\n";
        return out.str();
    }

    // Verifies `actual` (the JSON-serialized result of Runtime::snapshot()) against
    // the committed fixture, via verifySnapshot, and (re)writes a companion Mermaid
    // diagram of the graph's node connections to tests/snapshots/<name>.snapshot.md.
    inline void verifyGraphSnapshot(std::string const &name, std::string const &actual) {
        verifySnapshot(name, actual);

        std::ofstream mermaidOut(graphMermaidFilePath(name), std::ios::trunc);
        mermaidOut << graphMermaidDiagram(nlohmann::json::parse(actual));
    }
}
