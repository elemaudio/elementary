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
            scratchBuffers.push_back(std::vector<double>(maxBlockSize));

        for (int i = 0; i < (numInputChannels + numOutputChannels); ++i)
            scratchPointers.push_back(scratchBuffers[i].data());

        // Configure the runtime
        runtime = std::make_unique<elem::Runtime<double>>(sampleRate, maxBlockSize);

        // Register extension nodes
        runtime->registerNodeType("convolve", [](elem::NodeId const id, double fs, int const bs) {
            return std::make_shared<elem::ConvolutionNode<double>>(id, fs, bs);
        });

        runtime->registerNodeType("fft", [](elem::NodeId const id, double fs, int const bs) {
            return std::make_shared<elem::FFTNode<double>>(id, fs, bs);
        });

        runtime->registerNodeType("metro", [](elem::NodeId const id, double fs, int const bs) {
            return std::make_shared<elem::MetronomeNode<double>>(id, fs, bs);
        });

        runtime->registerNodeType("time", [](elem::NodeId const id, double fs, int const bs) {
            return std::make_shared<elem::SampleTimeNode<double>>(id, fs, bs);
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

    val addSharedResource(val name, val buffer)
    {
        auto n = emValToValue(name);
        auto buf = emValToValue(buffer);

        if (!n.isString()) {
            return valueToEmVal(elem::js::Object {
                {"success", false},
                {"message", "name must be a string type"},
            });
        }

        if (!buf.isFloat32Array()) {
            return valueToEmVal(elem::js::Object {
                {"success", false},
                {"message", "buffer must be a Float32Array"},
            });
        }

        auto& f32vec = buf.getFloat32Array();
        auto result = runtime->addSharedResource((elem::js::String) n, std::make_unique<elem::AudioBufferResource>(f32vec.data(), f32vec.size()));

        return valueToEmVal(elem::js::Object {
            {"success", result},
            {"message", result ? "Ok" : "cannot overwrite existing shared resource"},
        });
    }

    void pruneSharedResources()
    {
        runtime->pruneSharedResources();
    }

    val listSharedResources()
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
            const_cast<const double**>(scratchPointers.data()),
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
    std::unique_ptr<elem::Runtime<double>> runtime;
    std::vector<std::vector<double>> scratchBuffers;
    std::vector<double*> scratchPointers;

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
        .function("addSharedResource", &ElementaryAudioProcessor::addSharedResource)
        .function("pruneSharedResources", &ElementaryAudioProcessor::pruneSharedResources)
        .function("listSharedResources", &ElementaryAudioProcessor::listSharedResources)
        .function("process", &ElementaryAudioProcessor::process)
        .function("processQueuedEvents", &ElementaryAudioProcessor::processQueuedEvents)
        .function("setCurrentTime", &ElementaryAudioProcessor::setCurrentTime)
        .function("setCurrentTimeMs", &ElementaryAudioProcessor::setCurrentTimeMs);
};
