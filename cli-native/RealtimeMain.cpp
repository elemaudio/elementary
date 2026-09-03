#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <vector>

#include "graphs/GraphRegistry.h"
#include "elem/Renderer.h"
#include "elem/Runtime.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"


// A simple struct to proxy between the audio device and the Elementary engine
struct DeviceProxy {
    DeviceProxy(double sampleRate, size_t blockSize)
        : scratchData(2 * blockSize), runtime(std::make_shared<elem::Runtime<float>>(sampleRate, blockSize))
    {}

    void process(float* outputData, size_t numChannels, size_t numFrames)
    {
        // We might hit this the first time around, but after that should be fine
        if (scratchData.size() < (numChannels * numFrames))
            scratchData.resize(numChannels * numFrames);

        auto* deinterleaved = scratchData.data();
        std::array<float*, 2> ptrs {deinterleaved, deinterleaved + numFrames};

        runtime->process(
            nullptr,
            0,
            ptrs.data(),
            numChannels,
            numFrames,
            nullptr
        );

        for (size_t i = 0; i < numChannels; ++i)
        {
            for (size_t j = 0; j < numFrames; ++j)
            {
                outputData[i + numChannels * j] = deinterleaved[i * numFrames + j];
            }
        }
    }

    std::vector<float> scratchData;
    std::shared_ptr<elem::Runtime<float>> runtime;
};

void audioCallback(ma_device* pDevice, void* pOutput, const void* /* pInput */, ma_uint32 frameCount)
{
    auto* proxy = static_cast<DeviceProxy*>(pDevice->pUserData);

    const auto numChannels = static_cast<size_t>(pDevice->playback.channels);
    const auto numFrames = static_cast<size_t>(frameCount);

    proxy->process(static_cast<float*>(pOutput), numChannels, numFrames);
}

void printUsage(const char* programName)
{
    std::cout << "Usage: " << programName << " --graph <name>" << std::endl;
    std::cout << std::endl;
    std::cout << "Renders an Elementary audio graph to the default audio device." << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --graph <name>   Render the named graph" << std::endl;
    std::cout << "  --list           List the available graphs" << std::endl;
    std::cout << "  --help, -h       Show this usage information" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << programName << " --graph hello-sine" << std::endl;
}

void printGraphList()
{
    for (auto const& graphInfo : elem::lib::getGraphRegistry()) {
        std::cout << graphInfo.name << " — " << graphInfo.description << std::endl;
    }
}

int main(int argc, char** argv)
{
    std::string graphName;
    std::vector<std::string> args(argv + 1, argv + argc);

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--help" || args[i] == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (args[i] == "--list") {
            printGraphList();
            return 0;
        } else if (args[i] == "--graph") {
            if (i + 1 >= args.size()) {
                std::cerr << "Error: --graph requires a graph name" << std::endl;
                printUsage(argv[0]);
                return 1;
            }

            graphName = args[++i];
        }
    }

    if (graphName.empty()) {
        printUsage(argv[0]);
        return 1;
    }

    auto const& registry = elem::lib::getGraphRegistry();
    auto it = std::find_if(registry.begin(), registry.end(), [&graphName](elem::lib::GraphInfo const& graphInfo) {
        return graphInfo.name == graphName;
    });

    if (it == registry.end()) {
        std::cerr << "Error: unknown graph \"" << graphName << "\". Available graphs:" << std::endl;
        printGraphList();
        return 1;
    }

    ma_result result;

    ma_device_config deviceConfig;
    ma_device device;

    // XXX: I don't see a way to ask miniaudio for a specific block size. Let's just allocate
    // here for 1024 and resize in the first callback if we need to.
    std::unique_ptr<DeviceProxy> proxy = std::make_unique<DeviceProxy>(44100.0, 1024);

    deviceConfig = ma_device_config_init(ma_device_type_playback);

    deviceConfig.playback.pDeviceID = nullptr;
    deviceConfig.playback.format    = ma_format_f32;
    deviceConfig.playback.channels  = 2;
    deviceConfig.sampleRate         = 44100;
    deviceConfig.dataCallback       = audioCallback;
    deviceConfig.pUserData          = proxy.get();

    result = ma_device_init(nullptr, &deviceConfig, &device);

    if (result != MA_SUCCESS) {
        std::cout << "Failed to start the audio device! Exiting..." << std::endl;
        return 1;
    }

    // Build and render the graph before audio starts
    elem::Renderer<float> renderer(proxy->runtime);
    auto stats = renderer.renderGraph(it->build());

    std::cout << "Render result: " << stats.result << std::endl;
    std::cout << "Nodes added: " << stats.nodesAdded << std::endl;
    std::cout << "Edges added: " << stats.edgesAdded << std::endl;
    std::cout << "Props written: " << stats.propsWritten << std::endl;

    if (stats.result != elem::ReturnCode::Ok()) {
        std::cerr << "Failed to render graph: " << elem::ReturnCode::describe(stats.result) << std::endl;
        ma_device_uninit(&device);
        return 1;
    }

    result = ma_device_start(&device);

    if (result != MA_SUCCESS) {
        std::cout << "Failed to start the audio device! Exiting..." << std::endl;
        ma_device_uninit(&device);
        return 1;
    }

    std::cout << "Press Enter to exit..." << std::endl;
    getchar();

    ma_device_uninit(&device);
    return 0;
}
