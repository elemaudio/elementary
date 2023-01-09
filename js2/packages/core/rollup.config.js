import cjs from '@rollup/plugin-commonjs';
import dts from 'rollup-plugin-dts';
import { nodeResolve } from '@rollup/plugin-node-resolve';
import { terser } from 'rollup-plugin-terser';

export default [
  // Bundle code
  {
    input: 'dist/index.js',
    output: {
      file: './dist/main.min.js',
    },
    plugins: [
      cjs(),
      nodeResolve(),
      terser(),
    ],
  },
  // Bundle types
  {
    input: 'dist/index.d.ts',
    output: {
      file: './dist/main.min.d.ts',
    },
    plugins: [
      dts(),
    ],
  },
]
