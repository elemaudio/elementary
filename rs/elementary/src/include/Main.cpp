#include "JSON.h"
#include "Runtime.h"
#include "cxx.h"

typedef elem::Runtime<float> FloatRuntime;

std::shared_ptr<FloatRuntime> make_float_runtime(double sampleRate,
                                                 size_t blockSize) {
  return std::make_shared<FloatRuntime>(sampleRate, blockSize);
}

bool apply_instructions_float(std::shared_ptr<FloatRuntime> runtime,
                              u_int8_t const *instructions,
                              unsigned int length) {
  std::vector<u_int8_t> instr(instructions, instructions + length);

  // TODO Remove print debugging
  std::cout << "CBOR bytes:" << "\n";
  for (size_t i = 0; i < length; ++i) {
    std::cout << std::hex << static_cast<int>(instr[i]) << " ";
  }
  std::cout << "\n\n";

  // Parse CBOR to Value
  elem::js::Value val = elem::js::parseCBOR(instr);

  // TODO Remove print debugging
  std::cout << "Value parsed from CBOR:" << "\n" << val << "\n";

  // TODO Call on runtime to apply instructions

  return true;
}

bool update_shared_resource_map_float(std::shared_ptr<FloatRuntime> runtime,
                                      std::string const &name,
                                      float const *data, size_t size) {
  return runtime->updateSharedResourceMap(name, data, size);
}

rust::Vec<rust::String>
get_shared_resource_map_keys_float(std::shared_ptr<FloatRuntime> runtime) {
  elem::SharedResourceMap<float>::KeyViewType map =
      runtime->getSharedResourceMapKeys();
  rust::Vec<rust::String> keys;

  for (auto &key : runtime->getSharedResourceMapKeys()) {
    keys.push_back(std::string(key));
  }

  return keys;
}

void prune_shared_resource_map_float(std::shared_ptr<FloatRuntime> runtime) {
  runtime->pruneSharedResourceMap();
}
