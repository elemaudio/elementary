import {
  el,
  Renderer,
} from '..';


function identity(x) {
  return x;
}

test('hashing reflects outputChannel from child nodes', async function() {
  const core = new Renderer(identity);
  const appendChildInstructionType = 2;

  const graph = el.add(...el.mc.sampleseq2({
    path: '/v/path',
    channels: 2,
    duration: 2,
    seq: [
      {value: 1, time: 0},
    ],
  }, 1).map(xn => el.mul(0.5, xn)));

  const { result } = await core.render(graph);

  // Because we used an identity function for the Renderer's batch handling
  // callback above, the render result here is actually just the instruction
  // batch itself. Given the instruction batch, we're looking to confirm that
  // at least one of the appendChild instructions connects the second output
  // channel from the sampleseq2 node.
  //
  // This test is intended to demonstrate that both of the `el.mul` nodes above
  // the sampleseq node get visited during the graph traversal, meaning that they
  // have different hashes. This is important because they both have the same
  // children if we only consider those children's hashes. We must also consider
  // those children's output channels. When we do, we will see the correct visit
  // and the connection to that second output channel.
  expect(result.some((instruction) => {
    if (instruction[0] !== appendChildInstructionType) {
      return false;
    }

    const [type, parent, child, channel] = instruction;
    return (channel === 1);
  }));
});
