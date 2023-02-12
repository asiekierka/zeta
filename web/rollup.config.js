import babel from '@rollup/plugin-babel';
import banner2 from 'rollup-plugin-banner2';
import resolve from '@rollup/plugin-node-resolve';
import terser from '@rollup/plugin-terser';

const fs = require('fs');

export default {
  input: './src/index.js',
  plugins: [
    resolve(),
    babel({
      babelHelpers: 'bundled',
      exclude: 'node_modules/**' // only transpile our source code
    })
    ,terser()
    ,banner2(() => fs.readFileSync('banner.txt', {encoding: 'utf8'}))
  ],
  output: {
    file: 'zeta.js',
    format: 'iife' // use browser globals
  }
};
