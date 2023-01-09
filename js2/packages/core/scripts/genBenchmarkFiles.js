import {Renderer, el} from '../dist/index.js';
import {writeFileSync, statSync, mkdirSync} from 'fs';


// A generic helper for writing benchmark files
function addBenchmark(name, renderFn) {
  if (!statSync('snaps', {throwIfNoEntry: false})?.isDirectory()) {
    mkdirSync('snaps');
  }

  let core = new Renderer(44100, (batch) => {
    writeFileSync(`snaps/${name}.json`, JSON.stringify(batch, null, 2));
  });

  core.render(renderFn());
}

addBenchmark('snap_01_sine', function() {
  return el.cycle(440);
});

addBenchmark('snap_02_blepsaw', function() {
  return el.add(...Array.from({length: 8}).map(function(_, i) {
    return el.blepsaw(440 + i);
  }));
});

addBenchmark('snap_03_taps', function() {
  return el.add(
    el.tapOut({name: 'snap'}, 1),
    el.tapIn({name: 'snap'}),
  );
});
