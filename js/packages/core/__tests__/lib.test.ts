import {
  el,
} from '../index';


test('errors on graph construction', function() {
  // expect(() => {
  //   // Missing second argument, should throw
  //   let p = el.phasor(1);
  // }).toThrow();

  expect(() => {
    // @ts-expect-error invalid node type
    let p = el.mul(1, 2, '4');
  }).toThrow();
});
