#include "Bindings.h"

RuntimeBindings::RuntimeBindings(double sampleRate, size_t blockSize)
    : m_runtime(sampleRate, blockSize)
{}

int RuntimeBindings::apply_instructions(rust::string const& batch)
{
    return m_runtime.applyInstructions(elem::js::parseJSON((std::string) batch));
}

std::unique_ptr<RuntimeBindings> new_runtime_instance(double sampleRate, size_t blockSize) {
    return std::make_unique<RuntimeBindings>(sampleRate, blockSize);
}
