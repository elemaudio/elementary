#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "elem/AudioBufferResource.h"
#include "elem/Renderer.h"
#include "elem/Runtime.h"
#include "elem/NodeRepr.h"

#include "GraphSnapshotTestUtils.h"
#include "elem/lib/Core.h"
#include "elem/lib/Envelopes.h"
#include "elem/lib/Filters.h"
#include "elem/lib/Math.h"
#include "elem/lib/Mc.h"
#include "elem/lib/Oscillators.h"

// A few notes on scope, since this suite doesn't mirror the JS test file layout 1:1 --
// the graph reconciliation behavior these tests cover happens to fall out of a single
// C++ test structure rather than the several JS files (core.test.js, mc.test.js,
// lib.test.js, hashing.test.js) it was originally cross-checked against:
//
// - js/packages/core/__tests__/lib.test.js's single test ("errors on graph construction")
//   is not covered here: both of its assertions are compile-time-enforced in C++, not
//   runtime-throwable conditions.
//     - el.seq({}, 1) checks a missing required argument; elem::lib::seq has fixed arity
//       (props, trigger, reset) enforced by the compiler.
//     - el.mul(1, 2, '4') checks a string where a number is expected; ElemNode is
//       std::variant<NodeRepr, js::Number> with no implicit conversion from string literals.
//
// - js/packages/core/__tests__/hashing.test.js exists to verify that instruction *shape* is
//   independent of the hashing algorithm, via a custom HashlessRenderer that replaces real
//   hash values with sequential mask ids. The C++ Renderer has no delegate abstraction to
//   hook into for that -- it always uses real NodeId hashes directly -- so that property is
//   not tested here. RendersComposedSynthVoiceGraph below still exercises the same complex
//   composed graph construction (adsr, lowpass, seq, blepsaw, blepsquare, train) end-to-end,
//   just via the normal renderGraph + verifyGraphSnapshot pattern used throughout this file.

TEST(NativeRendererSnapshotTests, RendersBasicSineWave) {
    const auto runtime = std::make_shared<elem::Runtime<float> >(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    const auto result = renderer.renderGraph({elem::lib::cycle(440.0)});

    elem::test::verifyGraphSnapshot(
        "BasicSineWaveGraph",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result.nodesAdded, 6);
    EXPECT_EQ(result.edgesAdded, 5);
    EXPECT_EQ(result.propsWritten, 5);
    EXPECT_EQ(result.result, elem::ReturnCode::Ok());
}

TEST(NativeRendererSnapshotTests, NumericLiteralIsResolvedToConstantNode) {
    const auto runtime = std::make_shared<elem::Runtime<float> >(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    const auto result = renderer.renderGraph({
        elem::lib::sin(
            elem::lib::mul({
                2.0 * elem::lib::PI<float>,
                elem::lib::phasor(440.0)
            })
        )
    });

    elem::test::verifyGraphSnapshot(
        "BasicSineWaveGraphNumericLiteral",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result.nodesAdded, 6);
    EXPECT_EQ(result.edgesAdded, 5);
    EXPECT_EQ(result.propsWritten, 5);
    EXPECT_EQ(result.result, elem::ReturnCode::Ok());
}

TEST(NativeRendererSnapshotTests, DistinguishByProps) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    runtime->addSharedResource("test/path.wav", std::make_unique<elem::AudioBufferResource>(1, 512));
    elem::Renderer<float> renderer(runtime);

    auto renderVoice = [](std::string path, std::vector<elem::js::Number> seq) {
        return elem::NodeRepr::createNode("sample", elem::js::Object{{"path", path}}, {
            elem::NodeRepr::createNode("seq", elem::js::Object{{"seq", elem::js::Array(seq.begin(), seq.end())}}, {
                elem::lib::le(
                    elem::lib::phasor(elem::lib::constant(2.0)),
                    elem::lib::constant(0.5)
                )
            })
        });
    };

    const auto result = renderer.renderGraph({
        renderVoice("test/path.wav", {0, 0, 1}),
        renderVoice("test/path.wav", {0, 1, 0}),
    });

    elem::test::verifyGraphSnapshot(
        "DistinguishByProps",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result.nodesAdded, 10);
    EXPECT_EQ(result.edgesAdded, 9);
    EXPECT_EQ(result.propsWritten, 12);
    EXPECT_EQ(result.result, elem::ReturnCode::Ok());
}

TEST(NativeRendererSnapshotTests, MultiChannelBasics) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    // Run the same thing in two channels; we expect structural sharing except for the root nodes.
    const auto result = renderer.renderGraph({
        elem::lib::cycle(440.0),
        elem::lib::cycle(440.0),
    });

    elem::test::verifyGraphSnapshot(
        "MultiChannelBasics",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result.nodesAdded, 7);
    EXPECT_EQ(result.edgesAdded, 6);
    EXPECT_EQ(result.propsWritten, 8);
    EXPECT_EQ(result.result, elem::ReturnCode::Ok());
}

TEST(NativeRendererSnapshotTests, SimpleSharing) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    const auto result1 = renderer.renderGraph({elem::lib::cycle(440.0)});

    EXPECT_EQ(result1.nodesAdded, 6);
    EXPECT_EQ(result1.edgesAdded, 5);
    EXPECT_EQ(result1.propsWritten, 5);
    EXPECT_EQ(result1.result, elem::ReturnCode::Ok());

    // Second render inserts a tanh at the top; we should find the existing subtree and
    // share it, adding only the new tanh node and a new root (since the root's hash
    // depends on its child, which changed).
    const auto result2 = renderer.renderGraph({elem::lib::tanh(elem::lib::cycle(440.0))});

    elem::test::verifyGraphSnapshot(
        "SimpleSharing",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result2.nodesAdded, 2);
    EXPECT_EQ(result2.edgesAdded, 2);
    EXPECT_EQ(result2.propsWritten, 3);
    EXPECT_EQ(result2.result, elem::ReturnCode::Ok());
}

static elem::lib::NodeReprSPtr renderKeyedVoice(std::string key, double freq) {
    return elem::lib::sin(elem::lib::mul({
        elem::lib::constant(2.0 * elem::lib::PI<float>),
        elem::lib::phasor(elem::lib::constant(freq, key)),
    }));
}

TEST(NativeRendererSnapshotTests, DistinguishedSubtreesByKey) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    const auto result = renderer.renderGraph({
        elem::lib::add({
            renderKeyedVoice("fq1", 440),
            renderKeyedVoice("fq2", 440),
            renderKeyedVoice("fq3", 440),
            renderKeyedVoice("fq4", 440),
        })
    });

    elem::test::verifyGraphSnapshot(
        "DistinguishedSubtreesByKey",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result.nodesAdded, 19);
    EXPECT_EQ(result.edgesAdded, 21);
    EXPECT_EQ(result.propsWritten, 12);
    EXPECT_EQ(result.result, elem::ReturnCode::Ok());
}

TEST(NativeRendererSnapshotTests, StructuralEqualityWithValueChange) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    const auto result1 = renderer.renderGraph({
        elem::lib::add({
            renderKeyedVoice("fq1", 440),
            renderKeyedVoice("fq2", 440),
            renderKeyedVoice("fq3", 440),
            renderKeyedVoice("fq4", 440),
        })
    });

    EXPECT_EQ(result1.nodesAdded, 19);
    EXPECT_EQ(result1.edgesAdded, 21);
    EXPECT_EQ(result1.propsWritten, 12);
    EXPECT_EQ(result1.result, elem::ReturnCode::Ok());

    // Change one of the keyed values; we expect structural equality (no new nodes or
    // edges) since the node is found by its key, but the changed value is still written.
    const auto result2 = renderer.renderGraph({
        elem::lib::add({
            renderKeyedVoice("fq1", 441),
            renderKeyedVoice("fq2", 440),
            renderKeyedVoice("fq3", 440),
            renderKeyedVoice("fq4", 440),
        })
    });

    elem::test::verifyGraphSnapshot(
        "StructuralEqualityWithValueChange",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result2.nodesAdded, 0);
    EXPECT_EQ(result2.edgesAdded, 0);
    EXPECT_EQ(result2.propsWritten, 1);
    EXPECT_EQ(result2.result, elem::ReturnCode::Ok());
}

// Verifies that switching away from and back to a previously-rendered voice is a
// no-op: the voice's subtree persists in the runtime (never garbage collected) even
// after a different voice becomes active, so the third render finds it unchanged.
TEST(NativeRendererSnapshotTests, SwitchAndSwitchBack) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    const auto result1 = renderer.renderGraph({renderKeyedVoice("hi", 440)});
    EXPECT_EQ(result1.nodesAdded, 6);
    EXPECT_EQ(result1.edgesAdded, 5);
    EXPECT_EQ(result1.propsWritten, 6);
    EXPECT_EQ(result1.result, elem::ReturnCode::Ok());

    const auto result2 = renderer.renderGraph({renderKeyedVoice("bye", 880)});
    EXPECT_EQ(result2.nodesAdded, 5);
    EXPECT_EQ(result2.edgesAdded, 5);
    EXPECT_EQ(result2.propsWritten, 5);
    EXPECT_EQ(result2.result, elem::ReturnCode::Ok());

    // Third render switches back to A. We expect this to be a full no-op: A's subtree
    // was never garbage collected, so nothing new needs to be created or written.
    const auto result3 = renderer.renderGraph({renderKeyedVoice("hi", 440)});

    elem::test::verifyGraphSnapshot(
        "SwitchAndSwitchBack",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result3.nodesAdded, 0);
    EXPECT_EQ(result3.edgesAdded, 0);
    EXPECT_EQ(result3.propsWritten, 0);
    EXPECT_EQ(result3.result, elem::ReturnCode::Ok());
}

TEST(NativeRendererSnapshotTests, RefSetterUpdatesPropsWithoutRecreatingTree) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    // Sine tone with a frequency set by ref.
    auto ref = renderer.createRef("const", elem::js::Object{{"value", 440.0}}, {});

    const auto result1 = renderer.renderGraph({
        elem::lib::sin(elem::lib::mul({
            elem::lib::constant(2.0 * elem::lib::PI<float>),
            elem::lib::phasor(elem::lib::ElemNode(ref.node)),
        }))
    });

    EXPECT_EQ(result1.nodesAdded, 6);
    EXPECT_EQ(result1.edgesAdded, 5);
    EXPECT_EQ(result1.propsWritten, 6);
    EXPECT_EQ(result1.result, elem::ReturnCode::Ok());

    // Using our ref setter: we expect a single prop update, no structural change.
    const auto result2 = ref.setter(elem::js::Object{{"value", 550.0}});

    elem::test::verifyGraphSnapshot(
        "RefSetterUpdatesPropsWithoutRecreatingTree",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result2.nodesAdded, 0);
    EXPECT_EQ(result2.edgesAdded, 0);
    EXPECT_EQ(result2.propsWritten, 1);
    EXPECT_EQ(result2.result, elem::ReturnCode::Ok());
}

TEST(NativeRendererSnapshotTests, McHashingReflectsOutputChannelFromChildNodes) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    runtime->addSharedResource("/v/path", std::make_unique<elem::AudioBufferResource>(2, 512));
    elem::Renderer<float> renderer(runtime);

    auto channels = elem::lib::sampleseq2(
        elem::lib::MCSampleSeq2Props{
            {}, // key
            std::string("/v/path"), // path
            {{1.0, 0.0}}, // seq
            2.0, // duration
        },
        2.0,
        1.0
    );

    std::vector<elem::lib::ElemNode> muls;
    for (auto& channel : channels) {
        muls.push_back(elem::lib::mul({0.5, elem::lib::ElemNode(channel)}));
    }

    const auto result = renderer.renderGraph({elem::lib::add(std::move(muls))});

    const auto snapshotJson = elem::js::serialize(elem::js::Value(runtime->snapshot()));
    elem::test::verifyGraphSnapshot("McHashingReflectsOutputChannelFromChildNodes", snapshotJson);

    EXPECT_EQ(result.nodesAdded, 7);
    EXPECT_EQ(result.edgesAdded, 8);
    EXPECT_EQ(result.propsWritten, 8);
    EXPECT_EQ(result.result, elem::ReturnCode::Ok());

    // Demonstrates that both `mul` nodes above get visited/created independently during
    // traversal (they have different hashes because they address different output channels
    // of the same mc.sampleseq2 child), so the connection to the second output channel
    // survives in the rendered graph.
    const auto snapshot = nlohmann::json::parse(snapshotJson);

    std::string sampleseq2NodeId;
    for (auto const& [nodeId, node] : snapshot.items()) {
        if (node.value("kind", "") == "mc.sampleseq2") {
            sampleseq2NodeId = nodeId;
        }
    }
    ASSERT_FALSE(sampleseq2NodeId.empty());

    // Check both channel 0 and channel 1 connections out of the mc.sampleseq2 node
    // independently. If the two `mul` nodes above were to collapse into a single node
    // (the bug this test guards against), one of these two connections is overwritten
    // and disappears from the graph entirely -- so checking only one channel in
    // isolation isn't enough to catch the regression.
    bool foundChannelZeroInletFromSampleSeq2 = false;
    bool foundChannelOneInletFromSampleSeq2 = false;
    for (auto const& [nodeId, node] : snapshot.items()) {
        for (auto const& inlet : node.value("inlets", nlohmann::json::array())) {
            if (inlet.value("source", std::string()) == sampleseq2NodeId) {
                auto const channel = static_cast<int>(inlet.value("outletChannel", 0.0));

                if (channel == 0) foundChannelZeroInletFromSampleSeq2 = true;
                if (channel == 1) foundChannelOneInletFromSampleSeq2 = true;
            }
        }
    }
    EXPECT_TRUE(foundChannelZeroInletFromSampleSeq2);
    EXPECT_TRUE(foundChannelOneInletFromSampleSeq2);
}

TEST(NativeRendererSnapshotTests, RendersComposedSynthVoiceGraph) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    auto synthVoice = [](elem::lib::ElemNode hz) {
        return elem::lib::mul({
            0.25,
            elem::lib::add({
                elem::lib::blepsaw(elem::lib::mul({hz, 1.001})),
                elem::lib::blepsquare(elem::lib::mul({hz, 0.994})),
                elem::lib::blepsquare(elem::lib::mul({hz, 0.501})),
                elem::lib::blepsaw(elem::lib::mul({hz, 0.496})),
            }),
        });
    };

    auto train = elem::lib::train(4.8);

    // These are the equal-tempered frequencies for arp steps {0, 4, 7, 11, 12, 11, 7, 4}
    // relative to 261.63 * 0.5 Hz, i.e. 261.63 * 0.5 * pow(2.0, step / 12.0). They're
    // hardcoded (rather than computed with std::pow at test time) because std::pow can
    // round its last bit differently between Debug and Release builds, which changes the
    // node's structural hash (see HashUtils::hashProps) and cascades into every ancestor
    // node's ID, making this snapshot fail to match across build types despite the graph
    // being logically identical.
    std::vector<elem::js::Number> arp = {
        130.815,
        164.81657214199782,
        196.0010402616231,
        246.94583642691146,
        261.63,
        246.94583642691146,
        196.0010402616231,
        164.81657214199782,
    };

    auto modulate = [](elem::lib::ElemNode x, elem::lib::ElemNode rate, elem::lib::ElemNode amt) {
        return elem::lib::add({x, elem::lib::mul({amt, elem::lib::cycle(rate)})});
    };

    auto env = elem::lib::adsr(0.01, 0.5, 0.0, 0.4, train);

    auto filt = [&](elem::lib::ElemNode x) {
        return elem::lib::lowpass(
            elem::lib::add({40.0, elem::lib::mul({modulate(1840.0, 0.05, 1800.0), env})}),
            1.0,
            x
        );
    };

    auto seqNode = elem::lib::seq(
        elem::lib::SeqProps{{}, elem::js::Array(arp.begin(), arp.end()), {}, true}, // key, seq, offset, hold
        train,
        0.0
    );

    auto out = elem::lib::mul({0.25, filt(synthVoice(seqNode))});

    const auto result = renderer.renderGraph({out, out});

    elem::test::verifyGraphSnapshot(
        "RendersComposedSynthVoiceGraph",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result.result, elem::ReturnCode::Ok());
}

TEST(NativeRendererSnapshotTests, ChangingLeafPropRecreatesWholeTree) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    auto buildGraph = [](double freq) {
        return elem::lib::sin(elem::lib::mul({
            2.0 * elem::lib::PI<float>,
            elem::lib::phasor(freq),
        }));
    };

    const auto result1 = renderer.renderGraph({buildGraph(440.0)});
    ASSERT_EQ(result1.result, elem::ReturnCode::Ok());

    // Second render changes the frequency literal feeding the phasor leaf. Since
    // nothing here is keyed, the leaf's new hash cascades through every ancestor
    // (phasor -> mul -> sin -> root), so we expect the whole chain recreated.
    const auto result2 = renderer.renderGraph({buildGraph(441.0)});

    elem::test::verifyGraphSnapshot(
        "ChangingLeafPropRecreatesWholeTree",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result2.nodesAdded, 5);
    EXPECT_EQ(result2.edgesAdded, 5);
    EXPECT_EQ(result2.propsWritten, 4);
    EXPECT_EQ(result2.result, elem::ReturnCode::Ok());
}

TEST(NativeRendererSnapshotTests, ChangingMiddleNodeRedrawsOnlyParents) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    auto buildGraph = [](double cutoff) {
        return elem::lib::add({
            elem::lib::lowpass(cutoff, 1.0, elem::lib::phasor(440.0)),
            elem::lib::sin(elem::lib::phasor(220.0)),
        });
    };

    const auto result1 = renderer.renderGraph({buildGraph(1000.0)});
    ASSERT_EQ(result1.result, elem::ReturnCode::Ok());

    // Second render changes the cutoff feeding the middle `lowpass` node. Its
    // child (phasor(440)) and the unrelated sibling subtree (sin(phasor(220)))
    // are unchanged and should be found/shared; only `lowpass` and its ancestors
    // (add, root) should be recreated.
    const auto result2 = renderer.renderGraph({buildGraph(500.0)});

    elem::test::verifyGraphSnapshot(
        "ChangingMiddleNodeRedrawsOnlyParents",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result2.nodesAdded, 4);
    EXPECT_EQ(result2.edgesAdded, 6);
    EXPECT_EQ(result2.propsWritten, 5);
    EXPECT_EQ(result2.result, elem::ReturnCode::Ok());
}

TEST(NativeRendererSnapshotTests, AddingMiddleNodeRedrawsOnlyParents) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    const auto result1 = renderer.renderGraph({
        elem::lib::sin(elem::lib::phasor(440.0))
    });
    ASSERT_EQ(result1.result, elem::ReturnCode::Ok());

    // Second render splices a new `mul` node in between `sin` and its child
    // `phasor`. The phasor leaf is unchanged and should be found/shared; `mul`
    // is newly created, and `sin`/root are recreated because their child's
    // hash changed.
    const auto result2 = renderer.renderGraph({
        elem::lib::sin(elem::lib::mul({2.0, elem::lib::phasor(440.0)}))
    });

    elem::test::verifyGraphSnapshot(
        "AddingMiddleNodeRedrawsOnlyParents",
        elem::js::serialize(elem::js::Value(runtime->snapshot()))
    );

    EXPECT_EQ(result2.nodesAdded, 4);
    EXPECT_EQ(result2.edgesAdded, 4);
    EXPECT_EQ(result2.propsWritten, 4);
    EXPECT_EQ(result2.result, elem::ReturnCode::Ok());
}

TEST(NativeRendererSnapshotTests, GcCleansUpUnusedNodes) {
    const auto runtime = std::make_shared<elem::Runtime<float>>(44100.0, 512);
    elem::Renderer<float> renderer(runtime);

    // A root node fades out over 20ms rather than disappearing instantly, and
    // stays in Runtime::currentRoots (and therefore referenced by the live
    // render sequence) until that fade settles -- so we drive the clock
    // forward with real process() calls to let it finish.
    auto processBlock = [&]() {
        float outputBuffer[512] = {};
        float* outputChannels[1] = {outputBuffer};
        runtime->process(nullptr, 0, outputChannels, 1, 512, nullptr);
    };

    const auto result1 = renderer.renderGraph({renderKeyedVoice("hi", 440)});
    ASSERT_EQ(result1.result, elem::ReturnCode::Ok());
    // Process a block first so the root actually fades in, mirroring a live audio callback—this is what makes the fade-out observable below.
    processBlock();

    // Switch to a different keyed voice; "hi"'s root starts fading out but
    // remains active (and thus referenced) until its fade settles.
    const auto result2 = renderer.renderGraph({renderKeyedVoice("bye", 880)});
    ASSERT_EQ(result2.result, elem::ReturnCode::Ok());

    // 44100 * 0.02s = 882 samples for the 20ms fade-out; two 512-sample
    // blocks comfortably covers that.
    processBlock();
    processBlock();

    // Re-render "bye" (structurally unchanged). Since "hi" is still sitting
    // in currentRoots, this root set still differs from currentRoots, so the
    // renderer re-emits an activate-roots instruction; now that "hi"'s fade
    // has settled, it's finally dropped from currentRoots and excluded from
    // the next render sequence.
    const auto result3 = renderer.renderGraph({renderKeyedVoice("bye", 880)});
    ASSERT_EQ(result3.result, elem::ReturnCode::Ok());

    // Swap the new (hi-excluding) render sequence into place so the old one
    // -- still holding "hi"'s subtree -- is no longer referenced.
    processBlock();

    const auto snapshotJsonBeforeGc = elem::js::serialize(elem::js::Value(runtime->snapshot()));
    const auto snapshotBeforeGc = nlohmann::json::parse(snapshotJsonBeforeGc);

    const auto prunedNodeIds = runtime->gc();
    EXPECT_FALSE(prunedNodeIds.empty());

    const auto snapshotJsonAfterGc = elem::js::serialize(elem::js::Value(runtime->snapshot()));
    const auto snapshotAfterGc = nlohmann::json::parse(snapshotJsonAfterGc);

    elem::test::verifyGraphSnapshot("GcCleansUpUnusedNodes", snapshotJsonAfterGc);

    EXPECT_EQ(snapshotAfterGc.size(), snapshotBeforeGc.size() - prunedNodeIds.size());

    // The "hi" voice's frequency constant (440) should be gone; the still-active
    // "bye" voice's constant (880) should remain.
    bool foundFreq440 = false;
    bool foundFreq880 = false;
    for (auto const& [nodeId, node] : snapshotAfterGc.items()) {
        if (node.value("kind", "") == "const") {
            auto const value = node.at("props").value("value", 0.0);
            if (value == 440.0) foundFreq440 = true;
            if (value == 880.0) foundFreq880 = true;
        }
    }
    EXPECT_FALSE(foundFreq440);
    EXPECT_TRUE(foundFreq880);
}

// TODO: Create a test that exercises a custom node
