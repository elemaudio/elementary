#include "Runtime.h"
#include <memory>

typedef elem::Runtime<float> FloatRuntime;

std::shared_ptr<FloatRuntime> make_float_runtime(double sampleRate,
                                                 size_t blockSize) {
  return std::make_shared<FloatRuntime>(sampleRate, blockSize);
}

bool update_shared_resource_map_float(std::shared_ptr<FloatRuntime> runtime,
                                      std::string const &name,
                                      float const *data, size_t size) {
  return runtime->updateSharedResourceMap(name, data, size);
}
