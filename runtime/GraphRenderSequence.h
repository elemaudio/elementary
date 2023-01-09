#pragma once

#include <list>
#include <unordered_map>

#include "DefaultNodeTypes.h"


namespace elem
{

    template <typename FloatType>
    struct BlockContext
    {
        FloatType const** inputData;
        size_t numInputChannels;
        FloatType** outputData;
        size_t numOutputChannels;
        size_t numSamples;
        int64_t sampleTime;
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
        RootRenderSequence(std::unordered_map<GraphNodeId, FloatType*>& bm, std::shared_ptr<RootNode<FloatType>>& root)
            : rootPtr(root)
            , bufferMap(bm)
        {}

        void push(BufferAllocator<FloatType>& ba, std::shared_ptr<GraphNode<FloatType>>& node)
        {
            bufferMap.emplace(node->getId(), ba.next());

            renderOps.push_back([=, bufferMap = this->bufferMap](BlockContext<FloatType>& ctx) {
                auto* outputData = bufferMap.at(node->getId());
                node->process(ctx.inputData, outputData, ctx.numInputChannels, ctx.numSamples, ctx.sampleTime);
            });
        }

        void push(BufferAllocator<FloatType>& ba, std::shared_ptr<GraphNode<FloatType>>& node, std::vector<GraphNodeId> const& children)
        {
            bufferMap.emplace(node->getId(), ba.next());

            renderOps.push_back([=, bufferMap = this->bufferMap](BlockContext<FloatType>& ctx) {
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

                node->process(const_cast<const FloatType**>(ptrs.data()), outputData, numChildren, ctx.numSamples, ctx.sampleTime);
            });
        }

        void process(BlockContext<FloatType>& ctx)
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
        std::unordered_map<GraphNodeId, FloatType*>& bufferMap;

        using RenderOperation = std::function<void(BlockContext<FloatType>& context)>;
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
            int numInputChannels,
            FloatType** outputChannelData,
            int numOutputChannels,
            int numSamples,
            int64_t sampleTime)
        {
            BlockContext<FloatType> ctx {
                inputChannelData,
                static_cast<size_t>(numInputChannels),
                outputChannelData,
                static_cast<size_t>(numOutputChannels),
                static_cast<size_t>(numSamples),
                sampleTime,
            };

            // Clear the output channels
            for (int i = 0; i < numOutputChannels; ++i) {
                for (int j = 0; j < numSamples; ++j) {
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

        std::unordered_map<GraphNodeId, FloatType*> bufferMap;

    private:
        std::vector<std::shared_ptr<TapOutNode<FloatType>>> taps;
        std::vector<RootRenderSequence<FloatType>> subseqs;
    };

} // namespace elem
