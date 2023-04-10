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

    template <typename FloatType>
    class RootRenderSequence
    {
    public:
        RootRenderSequence(std::unordered_map<NodeId, FloatType*>& bm, std::shared_ptr<RootNode<FloatType>>& root)
            : rootPtr(root)
            , bufferMap(bm)
        {}

        void push(BufferAllocator<FloatType>& ba, std::shared_ptr<GraphNode<FloatType>>& node)
        {
            bufferMap.emplace(node->getId(), ba.next());

            renderOps.push_back([=, bufferMap = this->bufferMap](HostContext<FloatType>& ctx) {
                auto* outputData = bufferMap.at(node->getId());

                node->process(BlockContext<FloatType> {
                    ctx.inputData,
                    ctx.numInputChannels,
                    outputData,
                    ctx.numSamples,
                    ctx.userData,
                });
            });
        }

        void push(BufferAllocator<FloatType>& ba, std::shared_ptr<GraphNode<FloatType>>& node, std::vector<NodeId> const& children)
        {
            bufferMap.emplace(node->getId(), ba.next());

            renderOps.push_back([=, bufferMap = this->bufferMap](HostContext<FloatType>& ctx) {
                auto* outputData = bufferMap.at(node->getId());
                auto const numChildren = children.size();

                // This might actually be a great time for choc's SmallVector. We can allocate it outside the
                // lambda but capture it by copy (move). Initialize it with `numChildren` so that if `numChildren` exceeds
                // the stack-allocated space of choc's SmallVector then it performs its heap allocation before being
                // copied (moved) into the lambda capture group.
                std::array<FloatType*, 128> ptrs;

                for (size_t j = 0; j < numChildren; ++j) {
                    ptrs[j] = bufferMap.at(children[j]);
                }

                node->process(BlockContext<FloatType> {
                    const_cast<const FloatType**>(ptrs.data()),
                    numChildren,
                    outputData,
                    ctx.numSamples,
                    ctx.userData,
                });
            });
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
            auto* data = bufferMap.at(rootPtr->getId());

            for (size_t j = 0; j < ctx.numSamples; ++j) {
                ctx.outputData[outChan][j] += data[j];
            }
        }

    private:
        std::shared_ptr<RootNode<FloatType>> rootPtr;
        std::unordered_map<NodeId, FloatType*>& bufferMap;

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

        void pushTap(std::shared_ptr<TapOutNode<FloatType>> t) {
            taps.push_back(t);
        }

        void push(RootRenderSequence<FloatType>&& sq)
        {
            subseqs.push_back(std::move(sq));
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

            // Promote feedback tap buffers
            for (auto& ptr : taps) {
                ptr->promoteTapBuffers(numSamples);
            }

            // Process subsequences
            std::for_each(subseqs.begin(), subseqs.end(), [&](RootRenderSequence<FloatType>& sq) {
                sq.process(ctx);
            });
        }

        std::unordered_map<NodeId, FloatType*> bufferMap;

    private:
        std::vector<std::shared_ptr<TapOutNode<FloatType>>> taps;
        std::vector<RootRenderSequence<FloatType>> subseqs;
    };

} // namespace elem
