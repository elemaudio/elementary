#include <vector>
#include <iostream>

#include <choc_Files.h>
#include <choc_javascript.h>
#include <choc_javascript_QuickJS.h>

#include "Realtime.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"


const auto* kConsoleShimScript = R"script(
(function() {
  if (typeof globalThis.console === 'undefined') {
    globalThis.console = {
      log(...args) {
        return __log__('[log]', ...args);
      },
      warn(...args) {
        return __log__('[warn]', ...args);
      },
      error(...args) {
        return __log__('[error]', ...args);
      },
    };
  }
})();
)script";

// A simple struct to proxy between the audio device and the Elementary engine
struct DeviceProxy {
    DeviceProxy(double sampleRate, size_t blockSize)
        : scratchData(2 * blockSize), runtime(sampleRate, blockSize)
    {}

    void process(float* outputData, size_t numChannels, size_t numFrames)
    {
        // We might hit this the first time around, but after that should be fine
        if (scratchData.size() < (numChannels * numFrames))
            scratchData.resize(numChannels * numFrames);

        auto* deinterleaved = scratchData.data();
        std::array<float*, 2> ptrs {deinterleaved, deinterleaved + numFrames};

        // Elementary is happy to accept audio buffer data as an input to be
        // processed dynamically, such as applying effects, but here for simplicity
        // we're just going to produce output
        runtime.process(
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
    elem::Runtime<float> runtime;
};

// Our main audio processing callback from the miniaudio device
void audioCallback(ma_device* pDevice, void* pOutput, const void* /* pInput */, ma_uint32 frameCount)
{
    auto* proxy = static_cast<DeviceProxy*>(pDevice->pUserData);

    auto numChannels = static_cast<size_t>(pDevice->playback.channels);
    auto numFrames = static_cast<size_t>(frameCount);

    proxy->process(static_cast<float*>(pOutput), numChannels, numFrames);
}

int RealtimeMain(int argc, char** argv, std::function<void(elem::Runtime<float>&)> initCallback) {
    // First, initialize our audio device
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

    // Next, we'll initialize our JavaScript engine and establish a messaging channel by
    // defining a global callback function
    auto ctx = choc::javascript::createQuickJSContext();

    ctx.registerFunction("__postNativeMessage__", [&](choc::javascript::ArgumentList args) {
        proxy->runtime.applyInstructions(elem::js::parseJSON(args[0]->toString()));
        return choc::value::Value();
    });

    ctx.registerFunction("__log__", [](choc::javascript::ArgumentList args) {
        for (size_t i = 0; i < args.numArgs; ++i) {
            std::cout << choc::json::toString(*args[i], true) << std::endl;
        }

        return choc::value::Value();
    });

    initCallback(proxy->runtime);

    // Shim the js environment for console logging
    (void) ctx.evaluate(kConsoleShimScript);

    // Then we'll try to read the user's JavaScript file from disk
    if (argc < 2) {
        std::cout << "Missing argument: what file do you want to run?" << std::endl;
        return 1;
    }

    auto contents = choc::file::loadFileAsString(argv[1]);
    auto rv = ctx.evaluate(contents);

    // Finally, run the audio device
    ma_device_start(&device);

    std::cout << "Press Enter to exit..." << std::endl;
    getchar();

    ma_device_uninit(&device);
    return 0;
}
