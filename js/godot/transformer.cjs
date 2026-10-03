'use strict';
const path = require('path');
const {createRequire} = require('module');
const {adaptSource} = require('./source-adaptations.cjs');
module.exports.transform = args => {
  const projectRequire = createRequire(path.join(args.options.projectRoot, 'package.json'));
  return projectRequire('@react-native/metro-babel-transformer').transform({
    ...args, src: adaptSource(args.filename, args.src),
  });
};
module.exports.getCacheKey = () => require('crypto').createHash('sha256')
  .update(require('fs').readFileSync(__filename))
  .update(require('fs').readFileSync(require.resolve('./source-adaptations.cjs')))
  .update(require('fs').readFileSync(require.resolve('./source-adaptations.json')))
  .digest('hex');
