#pragma once

#include <elem/Runtime.h>
#include <memory>
#include <rust/cxx.h>
#include <string>


class RuntimeBindings {
public:
    RuntimeBindings(double sampleRate, size_t blockSize);

    int apply_instructions(rust::String const& batch);

private:
    elem::Runtime<float> m_runtime;
};

std::unique_ptr<RuntimeBindings> new_runtime_instance(double sampleRate, size_t blockSize);
