#pragma once

#include <list>
#include <unordered_map>

#include "DefaultNodeTypes.h"
#include "Types.h"


namespace elem
{

    //==============================================================================
    // A simple struct representing the audio processing inputs given to the runtime
    // by the host application.
    template <typename FloatType>
    struct HostContext
    {
        FloatType const** inputData;
        size_t numInputChannels;
        FloatType** outputData;
        size_t numOutputChannels;
        size_t numSamples;
        void* userData;
    };

    template <typename FloatType>
    class BufferAllocator
    {
    public:
        BufferAllocator(size_t blockSize)
            : blockSize(blockSize)
        {
            // We allocate buffer storage in chunks of 32 blocks
            storage.push_back(std::vector<FloatType>(32u * blockSize));
        }

        void reset()
        {
            nextChunk = 0;
            chunkOffset = 0;
        }

        FloatType* next()
        {
            if (nextChunk >= storage.size()) {
                storage.push_back(std::vector<FloatType>(32u * blockSize));
            }

            auto it = storage.begin();
            std::advance(it, nextChunk);

            auto& chunk = *it;
            auto* result = chunk.data() + chunkOffset;

            chunkOffset += blockSize;

            if (chunkOffset >= chunk.size()) {
                nextChunk++;
                chunkOffset = 0;
            }

            return result;
        }

    private:
        std::list<std::vector<FloatType>> storage;

        size_t blockSize = 0;
        size_t nextChunk = 0;
        size_t chunkOffset = 0;
    };

    struct BufferMapKeyHash {
        template <typename T1, typename T2>
        std::size_t operator() (std::pair<T1, T2> const& p) const {
            auto h1 = std::hash<T1>{}(p.first);
            auto h2 = std::hash<T2>{}(p.second);

            h1 ^= h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2);
            return h1;
        }
    };

    template <typename FloatType>
    class RootRenderSequence
    {
    public:
        RootRenderSequence(std::unordered_map<std::pair<NodeId, size_t>, FloatType*, BufferMapKeyHash>& bm, std::shared_ptr<RootNode<FloatType>>& root)
            : rootPtr(root)
            , bufferMap(bm)
        {}

        void push(BufferAllocator<FloatType>& ba, std::shared_ptr<GraphNode<FloatType>>& node)
        {
            // First we update our node and tap registry to make sure we can easily visit them
            // for tap promotion and event propagation
            nodeList.push_back(node);

            if (auto tap = std::dynamic_pointer_cast<TapOutNode<FloatType>>(node)) {
                tapList.push_back(tap);
            }

            // Next we prepare the render operation
            auto const numOuts = node->getNumOutputChannels();
            std::vector<FloatType*> outputPtrs(numOuts);

            for (size_t i = 0; i < numOuts; ++i) {
                bufferMap.insert({{node->getId(), i}, ba.next()});
                outputPtrs[i] = bufferMap.at({node->getId(), i});
            }
            
            renderOps.push_back([=, active = rootPtr->active(), outputPtrs = std::move(outputPtrs)](HostContext<FloatType>& ctx) mutable {
                node->process(BlockContext<FloatType> {
                    ctx.inputData,
                    ctx.numInputChannels,
                    outputPtrs.data(),
                    numOuts,
                    ctx.numSamples,
                    ctx.userData,
                    active,
                });
            });
        }

        void push(BufferAllocator<FloatType>& ba, std::shared_ptr<GraphNode<FloatType>>& node, std::vector<std::pair<NodeId, size_t>> const& children)
        {
            // First we update our node and tap registry to make sure we can easily visit them
            // for tap promotion and event propagation
            nodeList.push_back(node);

            if (auto tap = std::dynamic_pointer_cast<TapOutNode<FloatType>>(node)) {
                tapList.push_back(tap);
            }

            // Next we prepare the render operation
            auto const numOuts = node->getNumOutputChannels();
            std::vector<FloatType*> outputPtrs(numOuts);

            for (size_t i = 0; i < numOuts; ++i) {
                bufferMap.insert({{node->getId(), i}, ba.next()});
                outputPtrs[i] = bufferMap.at({node->getId(), i});
            }

            // Allocate room for the child pointers here, gets moved into the lambda capture group below
            std::vector<FloatType*> inputPtrs(children.size());
            auto const numChildren = children.size();

            for (size_t j = 0; j < numChildren; ++j) {
                inputPtrs[j] = bufferMap.at(children[j]);
            }
            
            renderOps.push_back([=, active = rootPtr->active(), outputPtrs = std::move(outputPtrs), inputPtrs = std::move(inputPtrs)](HostContext<FloatType>& ctx) mutable {
                node->process(BlockContext<FloatType> {
                    const_cast<const FloatType**>(inputPtrs.data()),
                    numChildren,
                    outputPtrs.data(),
                    numOuts,
                    ctx.numSamples,
                    ctx.userData,
                    active,
                });
            });
        }

        void processQueuedEvents(std::function<void(std::string const&, js::Value)>& evtCallback)
        {
            // We don't process events if our root is inactive
            if (rootPtr->getPropertyWithDefault("active", false))
            {
                for (auto& n : nodeList) {
                    n->processEvents(evtCallback);
                }
            }
        }

        void promoteTapBuffers(size_t numSamples)
        {
            // Don't promote if our RootRenderSequence represents a RootNode that has become
            // inactive, even if it's still fading out
            if (!rootPtr->active())
                return;

            for (auto& n : tapList) {
                n->promoteTapBuffers(numSamples);
            }
        }

        void process(HostContext<FloatType>& ctx)
        {
            size_t const outChan = rootPtr->getChannelNumber();

            // Nothing to do if this root has stopped running or if it's aimed at
            // an invalid output channel
            if (!rootPtr->stillRunning() || outChan < 0u || outChan >= ctx.numOutputChannels)
                return;

            // Run the subsequence
            for (size_t i = 0; i < renderOps.size(); ++i) {
                renderOps[i](ctx);
            }

            // Sum into the output buffer
            auto* data = bufferMap.at({rootPtr->getId(), 0});

            for (size_t j = 0; j < ctx.numSamples; ++j) {
                ctx.outputData[outChan][j] += data[j];
            }
        }

    private:
        std::shared_ptr<RootNode<FloatType>> rootPtr;
        std::vector<std::shared_ptr<GraphNode<FloatType>>> nodeList;
        std::vector<std::shared_ptr<TapOutNode<FloatType>>> tapList;
        std::unordered_map<std::pair<NodeId, size_t>, FloatType*, BufferMapKeyHash>& bufferMap;

        using RenderOperation = std::function<void(HostContext<FloatType>& context)>;
        std::vector<RenderOperation> renderOps;
    };

    template <typename FloatType>
    class GraphRenderSequence
    {
    public:
        GraphRenderSequence() = default;

        void reset()
        {
            subseqs.clear();
            bufferMap.clear();
        }

        void push(RootRenderSequence<FloatType>&& sq)
        {
            subseqs.push_back(std::move(sq));
        }

        void processQueuedEvents(std::function<void(std::string const&, js::Value)>&& evtCallback)
        {
            std::for_each(subseqs.begin(), subseqs.end(), [&](RootRenderSequence<FloatType>& sq) {
                sq.processQueuedEvents(evtCallback);
            });
        }

        void process(
            const FloatType** inputChannelData,
            size_t numInputChannels,
            FloatType** outputChannelData,
            size_t numOutputChannels,
            size_t numSamples,
            void* userData)
        {
            HostContext<FloatType> ctx {
                inputChannelData,
                numInputChannels,
                outputChannelData,
                numOutputChannels,
                numSamples,
                userData,
            };

            // Clear the output channels
            for (size_t i = 0; i < numOutputChannels; ++i) {
                for (size_t j = 0; j < numSamples; ++j) {
                    outputChannelData[i][j] = FloatType(0);
                }
            }

            // Process subsequences
            for (auto& sq : subseqs) {
                sq.process(ctx);
            }

            // Promote tap buffers.
            //
            // This step follows the processing step because we want read-then-write behavior
            // through the tap table for any feedback cycles. That means that, if we have a running
            // graph, and transition to a new graph, the tapIn node gets to read from the tap table,
            // which likely propagates to a corresponding tapOut node which can fill its internal delay
            // line. This then gets promoted in the subsequent step. If we went write-then-read, then
            // the new tapOut node would clobber whatever's in the tap table because it promotes before
            // it gets a chance to see what its corresponding tapIn is providing.
            for (auto& sq : subseqs) {
                sq.promoteTapBuffers(numSamples);
            }
        }

        std::unordered_map<std::pair<NodeId, size_t>, FloatType*, BufferMapKeyHash> bufferMap;

    private:
        std::vector<RootRenderSequence<FloatType>> subseqs;
    };

} // namespace elem
