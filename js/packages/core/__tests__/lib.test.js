import {
  el,
} from '..';


test('errors on graph construction', function() {
  expect(() => {
    // Missing second argument, should throw
    let p = el.phasor(1);
  }).toThrow();

  expect(() => {
    // Invalid node types
    let p = el.mul(1, 2, '4');
  }).toThrow();
});
