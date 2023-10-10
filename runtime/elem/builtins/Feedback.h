#pragma once

#include <functional>

#include "../GraphNode.h"
#include "../SingleWriterSingleReaderQueue.h"

#include "helpers/RefCountedPool.h"


namespace elem
{

    // A special graph node type for receiving feedback from within the graph.
    //
    // Will attempt to feed input from a shared mutable resource to its output. If
    // no resource is found by the given name, will emit silence.
    template <typename FloatType>
    struct TapInNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val, SharedResourceMap<FloatType>& resources) override
        {
            if (key == "name") {
                if (!val.isString())
                    return ReturnCode::InvalidPropertyType();

                auto ref = resources.getOrCreateMutable((js::String) val, GraphNode<FloatType>::getBlockSize());
                bufferQueue.push(std::move(ref));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto* outputData = ctx.outputData;
            auto numSamples = ctx.numSamples;

            while (bufferQueue.size() > 0) {
                bufferQueue.pop(activeBuffer);
            }

            if (!activeBuffer)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            std::copy_n(activeBuffer->data(), numSamples, outputData);
        }

        SingleWriterSingleReaderQueue<MutableSharedResourceBuffer<FloatType>> bufferQueue;
        MutableSharedResourceBuffer<FloatType> activeBuffer;
    };

    // A special graph node type for producing feedback from within the graph.
    //
    // Will pass its input through unaffected, while also buffering the same
    // signal into a circular buffer which should be drained by the GraphRenderer
    // via `promoteTapBuffers` and then served to the appropriate TapInNodes during
    // the render pass.
    template <typename FloatType>
    struct TapOutNode : public GraphNode<FloatType> {
        TapOutNode(NodeId id, FloatType const sr, int const blockSize)
            : GraphNode<FloatType>::GraphNode(id, sr, blockSize)
        {
            delayBuffer.resize(blockSize);
            std::fill_n(delayBuffer.data(), blockSize, FloatType(0));
        }

        int setProperty(std::string const& key, js::Value const& val, SharedResourceMap<FloatType>& resources) override
        {
            if (key == "name") {
                if (!val.isString())
                    return ReturnCode::InvalidPropertyType();

                auto ref = resources.getOrCreateMutable((js::String) val, GraphNode<FloatType>::getBlockSize());
                tapBufferQueue.push(std::move(ref));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void promoteTapBuffers(size_t numSamples) {
            // First order of business: grab the most recent feedback buffer to use if
            // there's anything in the queue. We do this here as opposed to in `process`
            // because we know that the GraphRenderer will call `promoteTapBuffers` before
            // running the graph traversal and calling `process`. Knowing that this step comes first,
            // we cycle in the appropriate active buffer here so that we have a chance to move the
            // read pointer before moving the write pointer below. This way the writer doesn't get
            // ahead of the reader, and the reader remains appropriately behind the writer.
            while (tapBufferQueue.size() > 0) {
                tapBufferQueue.pop(activeTapBuffer);
            }

            // If after the above we still don't have an active buffer, there's nothing to do
            if (activeTapBuffer == nullptr)
                return;

            // Here, we're good to go draining the buffered delay data into the tap line
            std::copy_n(delayBuffer.data(), numSamples, activeTapBuffer->data());
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // We can shortcut if there's nothing to fill the feedback buffer with
            if (numChannels < 1 || numSamples > delayBuffer.size())
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // Else, we write to our delay line and pass through our input
            auto* delayData = delayBuffer.data();

            std::copy_n(inputData[0], numSamples, delayData);
            std::copy_n(inputData[0], numSamples, outputData);
        }

        std::vector<FloatType> delayBuffer;
        SingleWriterSingleReaderQueue<MutableSharedResourceBuffer<FloatType>> tapBufferQueue;
        MutableSharedResourceBuffer<FloatType> activeTapBuffer;
    };

} // namespace elem
