import cjs from '@rollup/plugin-commonjs';
import replace from '@rollup/plugin-replace';
import typescript from '@rollup/plugin-typescript';

import { createFilter } from 'rollup-pluginutils';
import { minify } from 'terser';


/**
 * A quick plugin which loads the desired file, minifies it with terser, and then
 * returns the string itself.
 *
 * This is helpful so that I can load the wasm module and worklet processor into
 * the main bundle, inject them into a Blob and use the Blob URI to open the
 * worklet. Otherwise I would need a distinct bundle in some known spot on disk,
 * which makes the whole thingy difficult.
 */
function minString(opts = {}) {
  if (!opts.include) {
    throw Error('include option should be specified');
  }

  const filter = createFilter(opts.include, opts.exclude);

  return {
    name: 'minString',

    async transform(code, id) {
      if (filter(id)) {
        return {
          code: `export default ${JSON.stringify((await minify(code)).code)};`,
          map: { mappings: "" }
        };
      }
    }
  };
}

export default {
  input: 'index.ts',
  output: {
    dir: './dist/',
  },
  external: ['@elemaudio/core', 'events'],
  plugins: [
    // Must come first, otherwise rollup will inject some resolvey thingies that
    // I don't actually want in my raw strings
    minString({
      include: './raw/**/*.js',
    }),
    replace({
      preventAssignment: true,
      values: {
        'process.env.NODE_ENV': JSON.stringify('production'),
      },
    }),
    cjs(),
    typescript(),
  ],
};
