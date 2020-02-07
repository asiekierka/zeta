import babel from 'rollup-plugin-babel';
import banner from 'rollup-plugin-banner';
import resolve from 'rollup-plugin-node-resolve';
import { terser } from 'rollup-plugin-terser';

export default {
  input: './src/index.js',
  plugins: [
    resolve(),
    babel({
      exclude: 'node_modules/**' // only transpile our source code
    })
    ,terser()
    ,banner({
      file: 'banner.txt'
    })
  ],
  output: {
    file: 'zeta.js',
    format: 'iife', // use browser globals
    sourceMap: true
  }
};
