#include "Runtime.h"

typedef elem::Runtime<double> Runtime;

std::unique_ptr<Runtime> runtime_new(double sampleRate, size_t blockSize) {
  return std::make_unique<Runtime>(sampleRate, blockSize);
}
