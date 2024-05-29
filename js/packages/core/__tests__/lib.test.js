import {
  el,
} from '..';


test('errors on graph construction', function() {
  expect(() => {
    // Missing second argument, should throw
    let p = el.seq({}, 1);
  }).toThrow();

  expect(() => {
    // Invalid node types
    let p = el.mul(1, 2, '4');
  }).toThrow();
});

test('clamp', function() {
  let p = el.clamp(-0.1, 0, 1);
  expect(p).toMatchSnapshot();

  let p2 = el.clamp({}, -0.1, 0, 1);
  expect(p2).toMatchSnapshot();
})

test('lerp', function() {
  let p = el.lerp(0.4, 0, 1);
  expect(p).toMatchSnapshot();

  let p2 = el.lerp({}, 0.4, 0, 1);
  expect(p2).toMatchSnapshot();
})