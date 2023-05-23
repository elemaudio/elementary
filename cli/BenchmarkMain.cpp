#include <iostream>
#include <string>

#include "Benchmark.h"


int main(int argc, char **argv)
{
    // Read the input file from disk
    if (argc < 2) {
        std::cout << "Missing argument: what file do you want to run?" << std::endl;
        return 1;
    }

    auto inputFileName = std::string(argv[1]);

    runBenchmark<float>("Float", inputFileName, [](auto&) {});
    runBenchmark<double>("Double", inputFileName, [](auto&) {});

    return 0;
}
