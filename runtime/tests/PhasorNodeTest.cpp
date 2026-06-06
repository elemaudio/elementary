#include "elem/builtins/Core.h"

#include <cmath>
#include <iostream>

namespace {

template <typename FloatType>
elem::BlockContext<FloatType> makeContext(FloatType const** inputs, size_t inputChannels, FloatType** outputs, size_t numSamples)
{
    return elem::BlockContext<FloatType>{inputs, inputChannels, outputs, 1, numSamples, nullptr, true};
}

bool closeEnough(double actual, double expected)
{
    return std::abs(actual - expected) < 1.0e-12;
}

bool expectClose(double actual, double expected, char const* label)
{
    if (closeEnough(actual, expected)) {
        return true;
    }

    std::cerr << label << ": expected " << expected << ", got " << actual << "\n";
    return false;
}

bool phasorAdvancesByFrequencyOverSampleRate()
{
    elem::PhasorNode<double, false> phasor(1, 100.0, 4);

    double rate[] = {25.0, 25.0, 25.0, 25.0};
    double out[] = {0.0, 0.0, 0.0, 0.0};
    double const* inputs[] = {rate};
    double* outputs[] = {out};

    phasor.process(makeContext(inputs, 1, outputs, 4));

    return expectClose(out[0], 0.0, "phasor sample 0")
        && expectClose(out[1], 0.25, "phasor sample 1")
        && expectClose(out[2], 0.5, "phasor sample 2")
        && expectClose(out[3], 0.75, "phasor sample 3");
}

bool syncPhasorResetsOnRisingResetSignal()
{
    elem::PhasorNode<double, true> phasor(2, 100.0, 5);

    double rate[] = {25.0, 25.0, 25.0, 25.0, 25.0};
    double reset[] = {0.0, 0.0, 1.0, 1.0, 0.0};
    double out[] = {0.0, 0.0, 0.0, 0.0, 0.0};
    double const* inputs[] = {rate, reset};
    double* outputs[] = {out};

    phasor.process(makeContext(inputs, 2, outputs, 5));

    return expectClose(out[0], 0.0, "sync phasor sample 0")
        && expectClose(out[1], 0.25, "sync phasor sample 1")
        && expectClose(out[2], 0.0, "sync phasor sample 2")
        && expectClose(out[3], 0.25, "sync phasor sample 3")
        && expectClose(out[4], 0.5, "sync phasor sample 4");
}

} // namespace

int main()
{
    return phasorAdvancesByFrequencyOverSampleRate() && syncPhasorResetsOnRisingResetSignal() ? 0 : 1;
}
