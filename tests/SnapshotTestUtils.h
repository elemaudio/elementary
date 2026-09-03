#pragma once

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "elem/deps/json.hpp"

namespace elem::test {
    inline std::string snapshotFilePath(std::string const &name) {
        return std::string(NATIVE_RENDERER_TESTS_DIR) + "/snapshots/" + name + ".snapshot.json";
    }

    // Re-formats a compact JSON string with indentation, so committed snapshot
    // fixtures are easy to read and diff by hand.
    inline std::string prettyPrintJSON(std::string const &compactJson) {
        return nlohmann::json::parse(compactJson).dump(2) + "\n";
    }

    // Compares `actual` against a fixture file committed to source control at
    // tests/snapshots/<name>.snapshot.json.
    //
    // If the fixture doesn't exist yet, or the UPDATE_SNAPSHOTS environment variable
    // is set, the fixture is (re)written from `actual` and the check passes. Otherwise
    // the fixture content must match `actual` exactly.
    inline void verifySnapshot(std::string const &name, std::string const &actual) {
        auto const path = snapshotFilePath(name);
        auto const pretty = prettyPrintJSON(actual);
        std::ifstream existing(path);

        if (!existing.good() || std::getenv("UPDATE_SNAPSHOTS") != nullptr) {
            std::ofstream out(path, std::ios::trunc);
            out << pretty;
            return;
        }

        std::stringstream buffer;
        buffer << existing.rdbuf();

        EXPECT_EQ(buffer.str(), pretty)
            << "Snapshot mismatch for \"" << name << "\". If this change is expected, "
            << "rerun with UPDATE_SNAPSHOTS=1 to update " << path << " and review the diff.";
    }
}

