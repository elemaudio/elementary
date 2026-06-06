#include "elem/builtins/Core.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace {

volatile double benchmarkSink = 0.0;

struct BenchmarkResult {
    std::string name;
    double medianMicroseconds;
    double samplesPerSecond;
    double checksum;
};

template <typename FloatType>
elem::BlockContext<FloatType> makeContext(FloatType const** inputs, size_t inputChannels, FloatType** outputs, size_t numSamples)
{
    return elem::BlockContext<FloatType>{inputs, inputChannels, outputs, 1, numSamples, nullptr, true};
}

template <typename Fn>
BenchmarkResult runBenchmark(std::string const& name, size_t samplesPerBlock, size_t blocksPerRun, size_t runs, Fn&& fn)
{
    std::vector<double> elapsed;
    elapsed.reserve(runs);

    double checksum = 0.0;

    for (size_t run = 0; run < runs; ++run) {
        auto const t0 = std::chrono::steady_clock::now();
        checksum += fn();
        auto const t1 = std::chrono::steady_clock::now();

        elapsed.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
    }

    std::sort(elapsed.begin(), elapsed.end());
    auto const median = elapsed[elapsed.size() / 2];
    auto const samples = static_cast<double>(samplesPerBlock * blocksPerRun);
    benchmarkSink = checksum;

    return BenchmarkResult{name, median, samples / (median / 1'000'000.0), checksum};
}

BenchmarkResult benchmarkPhasor(size_t samplesPerBlock, size_t blocksPerRun, size_t runs)
{
    std::vector<double> rate(samplesPerBlock, 440.0);
    std::vector<double> output(samplesPerBlock, 0.0);
    double const* inputs[] = {rate.data()};
    double* outputs[] = {output.data()};

    elem::PhasorNode<double, false> phasor(1, 44'100.0, samplesPerBlock);
    auto ctx = makeContext(inputs, 1, outputs, samplesPerBlock);

    return runBenchmark("phasor", samplesPerBlock, blocksPerRun, runs, [&]() {
        double checksum = 0.0;

        for (size_t block = 0; block < blocksPerRun; ++block) {
            phasor.process(ctx);
            checksum += output[block % samplesPerBlock];
        }

        return checksum;
    });
}

BenchmarkResult benchmarkSyncPhasor(size_t samplesPerBlock, size_t blocksPerRun, size_t runs)
{
    std::vector<double> rate(samplesPerBlock, 440.0);
    std::vector<double> reset(samplesPerBlock, 0.0);
    std::vector<double> output(samplesPerBlock, 0.0);
    double const* inputs[] = {rate.data(), reset.data()};
    double* outputs[] = {output.data()};

    elem::PhasorNode<double, true> phasor(2, 44'100.0, samplesPerBlock);
    auto ctx = makeContext(inputs, 2, outputs, samplesPerBlock);

    return runBenchmark("sphasor", samplesPerBlock, blocksPerRun, runs, [&]() {
        double checksum = 0.0;

        for (size_t block = 0; block < blocksPerRun; ++block) {
            phasor.process(ctx);
            checksum += output[block % samplesPerBlock];
        }

        return checksum;
    });
}

void printResult(BenchmarkResult const& result)
{
    std::cout << result.name
              << ",median_us=" << result.medianMicroseconds
              << ",samples_per_second=" << result.samplesPerSecond
              << ",checksum=" << result.checksum
              << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    auto const samplesPerBlock = argc > 1 ? static_cast<size_t>(std::stoull(argv[1])) : size_t{512};
    auto const blocksPerRun = argc > 2 ? static_cast<size_t>(std::stoull(argv[2])) : size_t{1'000'000};
    auto const runs = argc > 3 ? static_cast<size_t>(std::stoull(argv[3])) : size_t{7};

    std::cout << "samples_per_block=" << samplesPerBlock
              << ",blocks_per_run=" << blocksPerRun
              << ",runs=" << runs << '\n';

    printResult(benchmarkPhasor(samplesPerBlock, blocksPerRun, runs));
    printResult(benchmarkSyncPhasor(samplesPerBlock, blocksPerRun, runs));

    return benchmarkSink == 0.123456789 ? 1 : 0;
}
