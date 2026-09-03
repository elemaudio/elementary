#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Benchmark.h"
#include "RendererBenchmarkScenario.h"
#include "RendererBenchmarks.h"

namespace {
    void printUsage() {
        std::cout <<
            "Usage:\n"
            "  elembench runtime <file.js>\n"
            "  elembench renderer js <file.js>\n"
            "  elembench renderer native <scenario-name>\n"
            "  elembench renderer native --list\n";
    }

    void runRuntimeBenchmark(std::string const& inputFileName) {
        runBenchmark<float>("Float", inputFileName, [](auto&) {});
        runBenchmark<double>("Double", inputFileName, [](auto&) {});
    }

    void runRendererJSBenchmark(std::string const& inputFileName) {
        auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
        auto [build, render] = benchmark::makeJSGraphFns(runtime, inputFileName);
        const benchmark::RendererBenchmarkScenario scenario("Renderer JS, " + inputFileName, std::move(build), std::move(render));
        scenario.runBenchmark();
    }

    void runRendererNativeBenchmark(std::string const& scenarioName) {
        auto const& scenarios = benchmark::nativeRendererScenarios();
        auto const it = scenarios.find(scenarioName);

        if (it == scenarios.end()) {
            throw std::invalid_argument("Unknown native renderer scenario: " + scenarioName);
        }

        auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
        auto [build, render] = it->second(runtime);
        const benchmark::RendererBenchmarkScenario scenario("Renderer Native, " + scenarioName, std::move(build), std::move(render));
        scenario.runBenchmark();
    }

    void listNativeRendererScenarios() {
        std::cout << "Available native renderer scenarios:" << std::endl;

        for (auto const& [name, factory] : benchmark::nativeRendererScenarios()) {
            (void) factory;
            std::cout << "  " << name << std::endl;
        }
    }
}

int main(int argc, char **argv)
{
#ifndef NDEBUG
    std::cerr << "warning: elembench was built without NDEBUG (a Debug build) -- "
                  "timings will include debug-logging overhead and unoptimized code, "
                  "and won't reflect real performance. Rebuild with -DCMAKE_BUILD_TYPE=Release."
              << std::endl << std::endl;
#endif

    std::vector<std::string> args(argv + 1, argv + argc);

    if (args.empty()) {
        printUsage();
        return 1;
    }

    try {
        auto const subcommand = args.at(0);

        if (subcommand == "runtime") {
            if (args.size() < 2) throw std::invalid_argument("Missing argument: what file do you want to run?");
            runRuntimeBenchmark(args.at(1));
            return 0;
        }

        if (subcommand == "renderer") {
            if (args.size() < 2) throw std::invalid_argument("Missing argument: js or native?");
            auto const& rendererKind = args.at(1);

            if (rendererKind == "js") {
                if (args.size() < 3) throw std::invalid_argument("Missing argument: what file do you want to run?");
                runRendererJSBenchmark(args.at(2));
                return 0;
            }

            if (rendererKind == "native") {
                if (args.size() >= 3 && args.at(2) == "--list") {
                    listNativeRendererScenarios();
                    return 0;
                }

                if (args.size() < 3) throw std::invalid_argument("Missing argument: which scenario do you want to run?");
                runRendererNativeBenchmark(args.at(2));
                return 0;
            }

            throw std::invalid_argument("Unknown renderer kind: " + rendererKind);
        }

        throw std::invalid_argument("Unknown subcommand: " + subcommand);
    } catch (std::exception const& e) {
        std::cout << e.what() << std::endl << std::endl;
        printUsage();
        return 1;
    }
}
