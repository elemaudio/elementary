import cjs from '@rollup/plugin-commonjs';
import ignore from 'rollup-plugin-ignore';
import replace from '@rollup/plugin-replace';
import typescript from '@rollup/plugin-typescript';


export default {
  input: 'index.ts',
  output: {
    dir: './dist/',
  },
  external: [
    '@elemaudio/core',
    'events',
    'invariant',
  ],
  plugins: [
    cjs(),
    // See https://github.com/emscripten-core/emscripten/issues/11792
    // ES6 output and Node.js are a bit wonky in Emscripten right now, these
    // hacks get us around it in a way that seems to work both in node and browser.
    replace({
      preventAssignment: true,
      values: {
        '__dirname': JSON.stringify('.'),
      },
    }),
    ignore(['path', 'fs']),
    typescript(),
  ],
};
