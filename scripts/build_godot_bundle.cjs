#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const {createRequire} = require('module');
const {stageAssets} = require('../js/godot/assets.cjs');
const {isForeignPlatformModule} = require('../js/godot/resolver.cjs');

function parseArgs(argv) {
  const options = {
    project: 'samples/view-text',
    entry: 'godot.entry.js',
    out: 'dist/godot.bundle.js',
    dev: false,
    minify: false,
  };
  for (let index = 0; index < argv.length; index += 1) {
    const option = argv[index];
    if (option === '--dev' || option === '--minify') {
      options[option.slice(2)] = argv[++index] === 'true';
    } else if (
      option === '--project' ||
      option === '--entry' ||
      option === '--out'
    ) {
      options[option.slice(2)] = argv[++index];
    } else {
      throw new Error('Unknown or incomplete option: ' + option);
    }
  }
  return options;
}

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

function collectSources(sourceMap) {
  const sources = [...(sourceMap.sources ?? [])];
  for (const section of sourceMap.sections ?? []) {
    sources.push(...collectSources(section.map ?? {}));
  }
  return sources;
}

async function main() {
  const options = parseArgs(process.argv.slice(2));
  const projectRoot = path.resolve(options.project);
  const output = path.resolve(projectRoot, options.out);
  const outputDirectory = path.dirname(output);
  const projectRequire = createRequire(path.join(projectRoot, 'package.json'));
  const Metro = projectRequire('metro');
  const config = require(path.join(projectRoot, 'metro.config.godot.js'));
  const temporaryRoot = fs.mkdtempSync(
    path.join(projectRoot, '.godot-build-'),
  );
  const temporaryBundle = path.join(temporaryRoot, path.basename(output));
  const sourceMap = temporaryBundle + '.map';
  try {
    const result = await Metro.runBuild(config, {
      assets: true,
      dev: options.dev,
      entry: options.entry,
      minify: options.minify,
      out: temporaryBundle,
      platform: 'godot',
      sourceMap: true,
      sourceMapOut: sourceMap,
    });
    const map = JSON.parse(fs.readFileSync(sourceMap, 'utf8'));
    const dependencies = [
      ...new Set(
        collectSources(map).map(source => path.resolve(projectRoot, source)),
      ),
    ].sort();
    const foreign = dependencies.filter(isForeignPlatformModule);
    if (foreign.length > 0) {
      throw new Error(
        'Godot bundle selected foreign platform modules:\n' +
          foreign.join('\n'),
      );
    }
    const stagedDist = path.join(temporaryRoot, 'dist');
    fs.mkdirSync(stagedDist, {recursive: true});
    if (fs.existsSync(outputDirectory)) {fs.cpSync(outputDirectory, stagedDist, {recursive: true});}
    const bundleName = path.basename(output);
    fs.renameSync(temporaryBundle, path.join(stagedDist, bundleName));
    fs.renameSync(sourceMap, path.join(stagedDist, bundleName + '.map'));
    fs.writeFileSync(
      path.join(stagedDist, bundleName + '.dependencies.json'),
      JSON.stringify(
        dependencies.map(file =>
          path.relative(projectRoot, file).replaceAll(path.sep, '/'),
        ),
        null,
        2,
      ) + '\n',
    );
    const manifest = stageAssets(result.assets ?? [], stagedDist);
    fs.writeFileSync(
      path.join(stagedDist, 'assets-manifest.json'),
      JSON.stringify(manifest, null, 2) + '\n',
    );
    replaceDirectory(stagedDist, outputDirectory);
  } finally {
    fs.rmSync(temporaryRoot, {recursive: true, force: true});
  }
}

main().catch(error => {
  console.error(error.stack ?? error);
  process.exitCode = 1;
});
