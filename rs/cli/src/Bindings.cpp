#include "Bindings.h"
#include <algorithm>
#include <array>

// Our main audio processing callback from the audio device
void audioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    auto* bindings = static_cast<RuntimeBindings*>(pDevice->pUserData);

    auto numChannels = static_cast<size_t>(pDevice->playback.channels);
    auto numFrames = static_cast<size_t>(frameCount);

    bindings->process(
        static_cast<float const*>(pInput),
        static_cast<float*>(pOutput),
        numChannels,
        numFrames
    );
}

RuntimeBindings::RuntimeBindings(double sampleRate, size_t blockSize)
    : m_runtime(sampleRate, blockSize)
{
    // Initialize our audio device
    ma_result result;
    ma_device_config deviceConfig;

    deviceConfig = ma_device_config_init(ma_device_type_playback);

    deviceConfig.playback.pDeviceID = nullptr;
    deviceConfig.playback.format    = ma_format_f32;
    deviceConfig.playback.channels  = 2;
    deviceConfig.sampleRate         = sampleRate;
    deviceConfig.dataCallback       = audioCallback;
    deviceConfig.pUserData          = this;

    result = ma_device_init(nullptr, &deviceConfig, &m_device);

    if (result == MA_SUCCESS) {
        // Start the device
        ma_device_start(&m_device);
    }
}

RuntimeBindings::~RuntimeBindings()
{
    ma_device_uninit(&m_device);
}

int RuntimeBindings::apply_instructions(rust::string const& batch)
{
    return m_runtime.applyInstructions(elem::js::parseJSON((std::string) batch));
}

void RuntimeBindings::process(const float* inputData, float* outputData, size_t numChannels, size_t numFrames)
{
    // We might hit this the first time around, but after that should be fine
    if (scratchData.size() < (numChannels * numFrames))
        scratchData.resize(numChannels * numFrames);

    auto* deinterleaved = scratchData.data();
    std::array<float*, 2> ptrs {deinterleaved, deinterleaved + numFrames};

    m_runtime.process(
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

std::unique_ptr<RuntimeBindings> new_runtime_instance(double sampleRate, size_t blockSize) {
    return std::make_unique<RuntimeBindings>(sampleRate, blockSize);
}
