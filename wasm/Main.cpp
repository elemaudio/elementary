#include <emscripten/bind.h>

#include <memory>
#include <elem/Runtime.h>

#include "Convolve.h"
#include "FFT.h"
#include "Metro.h"
#include "SampleTime.h"


using namespace emscripten;

//==============================================================================
/** The main processor for the WASM DSP. */
class ElementaryAudioProcessor
{
public:
    //==============================================================================
    ElementaryAudioProcessor(int numIns, int numOuts)
    {
        numInputChannels = static_cast<size_t>(numIns);
        numOutputChannels = static_cast<size_t>(numOuts);
    }

    ~ElementaryAudioProcessor() = default;

    //==============================================================================
    /** Called before processing starts. */
    void prepare (double sr, unsigned int maxBlockSize)
    {
        sampleRate = sr;

        scratchBuffers.clear();
        scratchPointers.clear();

        for (int i = 0; i < (numInputChannels + numOutputChannels); ++i)
            scratchBuffers.push_back(std::vector<float>(maxBlockSize));

        for (int i = 0; i < (numInputChannels + numOutputChannels); ++i)
            scratchPointers.push_back(scratchBuffers[i].data());

        // Configure the runtime
        runtime = std::make_unique<elem::Runtime<float>>(sampleRate, maxBlockSize);

        // Register extension nodes
        runtime->registerNodeType("convolve", [](elem::NodeId const id, double fs, int const bs) {
            return std::make_shared<elem::ConvolutionNode<float>>(id, fs, bs);
        });

        runtime->registerNodeType("fft", [](elem::NodeId const id, double fs, int const bs) {
            return std::make_shared<elem::FFTNode<float>>(id, fs, bs);
        });

        runtime->registerNodeType("metro", [](elem::NodeId const id, double fs, int const bs) {
            return std::make_shared<elem::MetronomeNode<float>>(id, fs, bs);
        });

        runtime->registerNodeType("time", [](elem::NodeId const id, double fs, int const bs) {
            return std::make_shared<elem::SampleTimeNode<float>>(id, fs, bs);
        });
    }

    //==============================================================================
    /** Returns a Float32Array view into the internal work buffer data. */
    val getInputBufferData (int index)
    {
        auto len = scratchBuffers[index].size();
        auto* data = scratchBuffers[index].data();

        return val(typed_memory_view(len, data));
    }

    /** Returns a Float32Array view into the internal work buffer data. */
    val getOutputBufferData (int index)
    {
        auto len = scratchBuffers[numInputChannels + index].size();
        auto* data = scratchBuffers[numInputChannels + index].data();

        return val(typed_memory_view(len, data));
    }

    //==============================================================================
    /** Message batch handling. */
    val postMessageBatch (val payload)
    {
        auto v = emValToValue(payload);

        if (!v.isArray()) {
            return valueToEmVal(elem::js::Object {
                {"success", false},
                {"message", "Malformed message batch"},
            });
        }

        auto const& batch = v.getArray();
        auto const rc = runtime->applyInstructions(batch);

        return valueToEmVal(elem::js::Object {
            {"success", rc == elem::ReturnCode::Ok()},
            {"message", elem::ReturnCode::describe(rc)},
        });
    }

    void reset()
    {
        runtime->reset();
    }

    bool updateSharedResourceMap(val path, val buffer, val errorCallback)
    {
        auto p = emValToValue(path);
        auto b = emValToValue(buffer);

        if (!p.isString()) {
            errorCallback(val("Path must be a string type"));
            return false;
        }

        if (!b.isArray() && !b.isFloat32Array()) {
            errorCallback(val("Buffer argument must be an Array or Float32Array type"));
            return false;
        }

        if (b.isArray()) {
            if (auto f32vec = arrayToFloatVector(b.getArray())) {
                return runtime->updateSharedResourceMap((elem::js::String) p, f32vec->data(), f32vec->size());
            }
        } else {
            auto& f32vec = b.getFloat32Array();
            return runtime->updateSharedResourceMap((elem::js::String) p, f32vec.data(), f32vec.size());
        }

        return false;
    }

    void pruneSharedResourceMap()
    {
        runtime->pruneSharedResourceMap();
    }

    val listSharedResourceMap()
    {
        auto ret = val::array();
        size_t i = 0;

        for (auto& k : runtime->getSharedResourceMapKeys()) {
            ret.set(i++, val(k));
        }

        return ret;
    }

    /** Audio block processing. */
    void process (int const numSamples)
    {
        // We just operate on our scratch data. Expect the JavaScript caller to hit
        // our getInputBufferData and getOutputBufferData to prepare and extract the actual
        // data for this processor
        runtime->process(
            const_cast<const float**>(scratchPointers.data()),
            numInputChannels,
            scratchPointers.data() + numInputChannels,
            numOutputChannels,
            numSamples,
            static_cast<void*>(&sampleTime)
        );

        sampleTime += static_cast<int64_t>(numSamples);
    }

    /** Callback events. */
    void processQueuedEvents(val callback)
    {
        elem::js::Array batch;

        runtime->processQueuedEvents([this, &batch](std::string const& type, elem::js::Value evt) {
            batch.push_back(elem::js::Object({
                {"type", type},
                {"event", evt}
            }));
        });

        callback(valueToEmVal(batch));
    }

    void setCurrentTime(int const timeInSamples)
    {
        sampleTime = timeInSamples;
    }

    void setCurrentTimeMs(double const timeInMs)
    {
        double const timeInSeconds = timeInMs / 1000.0;
        sampleTime = static_cast<int64_t>(timeInSeconds * sampleRate);
    }

private:
    //==============================================================================
    std::optional<std::vector<float>> arrayToFloatVector (elem::js::Array const& ar)
    {
        std::vector<float> ret (ar.size());

        for (size_t i = 0; i < ar.size(); ++i) {
            if (!ar[i].isNumber()) {
                return {};
            }

            ret[i] = static_cast<float>((elem::js::Number) ar[i]);
        }

        return ret;
    }

    elem::js::Value emValToValue (val const& v)
    {
        if (v.isUndefined())
            return elem::js::Undefined();
        if (v.isNull())
            return elem::js::Null();
        if (v.isTrue())
            return elem::js::Value(true);
        if (v.isFalse())
            return elem::js::Value(false);
        if (v.isNumber())
            return elem::js::Value(v.as<double>());
        if (v.isString())
            return elem::js::Value(v.as<std::string>());
        if (v.instanceof(val::global("Float32Array"))) {
            // This conversion function is part of the emscripten namespace for
            // mapping from emscripten::val to a simple std::vector.
            return elem::js::Value(convertJSArrayToNumberVector<float>(v));
        }

        if (v.isArray())
        {
            auto const length = v["length"].as<int>();
            elem::js::Array ret;

            for (int i = 0; i < length; ++i)
            {
                ret.push_back(emValToValue(v[i]));
            }

            return ret;
        }

        // We don't support functions yet...
        if (v.instanceof(val::global("Function"))) {
            return elem::js::Undefined();
        }

        // This case must come at the end, because Arrays, Functions, Float32Arrays,
        // etc are all Objects too
        if (v.instanceof(val::global("Object"))) {
            auto const keys = val::global("Object").call<val>("keys", v);
            auto const numKeys = keys["length"].as<size_t>();

            elem::js::Object ret;

            for (size_t i = 0; i < numKeys; ++i) {
                ret.insert({keys[i].as<std::string>(), emValToValue(v[keys[i]])});
            }

            return ret;
        }

        return elem::js::Undefined();
    }

    val valueToEmVal (elem::js::Value const& v)
    {
        if (v.isUndefined())
            return val::undefined();
        if (v.isNull())
            return val::null();
        if (v.isBool())
            return val((bool) v);
        if (v.isNumber())
            return val((elem::js::Number) v);
        if (v.isString())
            return val((elem::js::String) v);

        if (v.isArray())
        {
            auto& va = v.getArray();
            auto ret = val::array();

            for (size_t i = 0; i < va.size(); ++i)
            {
                ret.set(i, valueToEmVal(va[i]));
            }

            return ret;
        }

        if (v.isFloat32Array())
        {
            auto& va = v.getFloat32Array();

            auto opts = val::object();
            opts.set("length", va.size());

            auto ret = val::global("Float32Array").call<val>("from", opts);

            for (size_t i = 0; i < va.size(); ++i)
            {
                ret.set(i, val(va[i]));
            }

            return ret;
        }

        if (v.isObject())
        {
            auto& vo = v.getObject();
            auto ret = val::object();

            for (auto const& [key, x] : vo)
            {
                ret.set(key, valueToEmVal(x));
            }

            return ret;
        }

        // Function types not supported!
        return val::undefined();
    }

    //==============================================================================
    std::unique_ptr<elem::Runtime<float>> runtime;
    std::vector<std::vector<float>> scratchBuffers;
    std::vector<float*> scratchPointers;

    int64_t sampleTime = 0;
    double sampleRate = 0;

    size_t numInputChannels = 0;
    size_t numOutputChannels = 2;
};

EMSCRIPTEN_BINDINGS(Elementary) {
    class_<ElementaryAudioProcessor>("ElementaryAudioProcessor")
        .constructor<int, int>()
        .function("prepare", &ElementaryAudioProcessor::prepare)
        .function("getInputBufferData", &ElementaryAudioProcessor::getInputBufferData)
        .function("getOutputBufferData", &ElementaryAudioProcessor::getOutputBufferData)
        .function("postMessageBatch", &ElementaryAudioProcessor::postMessageBatch)
        .function("reset", &ElementaryAudioProcessor::reset)
        .function("updateSharedResourceMap", &ElementaryAudioProcessor::updateSharedResourceMap)
        .function("pruneSharedResourceMap", &ElementaryAudioProcessor::pruneSharedResourceMap)
        .function("listSharedResourceMap", &ElementaryAudioProcessor::listSharedResourceMap)
        .function("process", &ElementaryAudioProcessor::process)
        .function("processQueuedEvents", &ElementaryAudioProcessor::processQueuedEvents)
        .function("setCurrentTime", &ElementaryAudioProcessor::setCurrentTime)
        .function("setCurrentTimeMs", &ElementaryAudioProcessor::setCurrentTimeMs);
};
