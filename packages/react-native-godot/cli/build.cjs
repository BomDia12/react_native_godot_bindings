'use strict';

const fs = require('fs');
const path = require('path');
const {stageAssets} = require('../assets.cjs');
const {isForeignPlatformModule} = require('../resolver.cjs');

const ENTRY_EXTENSIONS = ['.js', '.jsx', '.ts', '.tsx'];

// Entries are named without their extension in Project Settings, as Metro
// URLs name them.
function findEntry(projectRoot, entry) {
  if (fs.existsSync(path.join(projectRoot, entry))) {
    return entry;
  }
  const extension = ENTRY_EXTENSIONS.find(candidate => fs.existsSync(path.join(projectRoot, entry + candidate)));
  if (extension == null) {
    throw new Error(`No entry file ${entry}{${ENTRY_EXTENSIONS.join(',')}} in ${projectRoot}`);
  }
  return entry + extension;
}

// Swaps the output directory in one step, so Godot never sees half a build.
function replaceDirectory(source, destination) {
  const backup = destination + '.backup-' + process.pid;
  if (fs.existsSync(backup)) {
    fs.rmSync(backup, {recursive: true, force: true});
  }
  if (fs.existsSync(destination)) {
    fs.renameSync(destination, backup);
  }
  try {
    fs.renameSync(source, destination);
    fs.rmSync(backup, {recursive: true, force: true});
  } catch (error) {
    fs.rmSync(destination, {recursive: true, force: true});
    if (fs.existsSync(backup)) {
      fs.renameSync(backup, destination);
    }
    throw error;
  }
}

function isWithin(directory, file) {
  const relative = path.relative(directory, file);
  return relative !== '..' && !relative.startsWith('..' + path.sep) && !path.isAbsolute(relative);
}

function collectSources(sourceMap) {
  const sources = [...(sourceMap.sources ?? [])];
  for (const section of sourceMap.sections ?? []) {
    sources.push(...collectSources(section.map ?? {}));
  }
  return sources;
}

// Builds the bundle with its source map, the list of files it was built from,
// and its assets staged beside it. Other files in the output directory, such
// as other bundles, are kept.
async function buildBundle({projectRoot, entry, out, dev = false, minify = false}) {
  const output = path.resolve(projectRoot, out);
  const outputDirectory = path.dirname(output);
  if (isWithin(outputDirectory, projectRoot)) {
    throw new Error(`The bundle ${output} must be in a directory of its own, such as dist/: a build replaces its whole directory, which here contains the JavaScript project ${projectRoot}.`);
  }
  const Metro = require('metro');
  const config = require(path.join(projectRoot, 'metro.config.js'));
  const cacheRoot = path.join(projectRoot, 'node_modules', '.cache', 'react-native-godot');
  fs.mkdirSync(cacheRoot, {recursive: true});
  const temporaryRoot = fs.mkdtempSync(path.join(cacheRoot, 'build-'));
  const temporaryBundle = path.join(temporaryRoot, path.basename(output));
  const sourceMap = temporaryBundle + '.map';
  try {
    const result = await Metro.runBuild(config, {
      assets: true,
      dev,
      entry: findEntry(projectRoot, entry),
      minify,
      out: temporaryBundle,
      platform: 'godot',
      sourceMap: true,
      sourceMapOut: sourceMap,
    });
    const map = JSON.parse(fs.readFileSync(sourceMap, 'utf8'));
    const dependencies = [...new Set(collectSources(map).map(source => path.resolve(projectRoot, source)))].sort();
    const foreign = dependencies.filter(isForeignPlatformModule);
    if (foreign.length > 0) {
      throw new Error('Godot bundle selected foreign platform modules:\n' + foreign.join('\n'));
    }
    const stagedOutput = path.join(temporaryRoot, 'output');
    fs.mkdirSync(stagedOutput, {recursive: true});
    if (fs.existsSync(outputDirectory)) {
      fs.cpSync(outputDirectory, stagedOutput, {recursive: true});
    }
    const bundleName = path.basename(output);
    fs.renameSync(temporaryBundle, path.join(stagedOutput, bundleName));
    fs.renameSync(sourceMap, path.join(stagedOutput, bundleName + '.map'));
    fs.writeFileSync(
      path.join(stagedOutput, bundleName + '.dependencies.json'),
      JSON.stringify(dependencies.map(file => path.relative(projectRoot, file).replaceAll(path.sep, '/')), null, 2) + '\n',
    );
    const manifest = stageAssets(result.assets ?? [], stagedOutput);
    fs.writeFileSync(path.join(stagedOutput, 'assets-manifest.json'), JSON.stringify(manifest, null, 2) + '\n');
    fs.mkdirSync(path.dirname(outputDirectory), {recursive: true});
    replaceDirectory(stagedOutput, outputDirectory);
  } finally {
    fs.rmSync(temporaryRoot, {recursive: true, force: true});
  }
  return output;
}

module.exports = {buildBundle, findEntry};
