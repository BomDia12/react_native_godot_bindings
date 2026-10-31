'use strict';

const crypto = require('crypto');
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

// Names the set of modules a bundle holds. A segment only works with a base
// bundle that has the modules it was built against.
function fingerprint(dependencies) {
  return crypto.createHash('sha256').update(JSON.stringify(dependencies)).digest('hex').slice(0, 16);
}

// A segment holds only the modules its base bundle lacks, without Metro's
// prelude and polyfills, and runs its entry when the base bundle's runtime
// evaluates it.
function segmentConfig(config, projectRoot, base) {
  const dependenciesFile = base + '.dependencies.json';
  if (!fs.existsSync(dependenciesFile)) {
    throw new Error(`Build the bundle ${base} before its segments.`);
  }
  const inBase = new Set(JSON.parse(fs.readFileSync(dependenciesFile, 'utf8')).map(file => path.resolve(projectRoot, file)));
  const {mergeConfig} = require('@react-native/metro-config');
  return mergeConfig(config, {
    serializer: {
      getModulesRunBeforeMainModule: () => [],
      processModuleFilter: module => module.path !== '__prelude__' && module.output.every(output => output.type !== 'js/script') && !inBase.has(module.path),
    },
  });
}

// Builds the bundle with its source map, the list of files it was built from,
// and its assets staged beside it. Other files in the output directory, such
// as other bundles, are kept. With a base bundle, it builds a segment of it.
async function buildBundle({projectRoot, entry, out, dev = false, minify = false, base}) {
  const output = path.resolve(projectRoot, out);
  const outputDirectory = path.dirname(output);
  if (isWithin(outputDirectory, projectRoot)) {
    throw new Error(`The bundle ${output} must be in a directory of its own, such as dist/: a build replaces its whole directory, which here contains the JavaScript project ${projectRoot}.`);
  }
  const Metro = require('metro');
  const projectConfig = require(path.join(projectRoot, 'metro.config.js'));
  const config = base ? segmentConfig(projectConfig, projectRoot, path.resolve(projectRoot, base)) : projectConfig;
  // Staging beside the output keeps the final renames on one file system.
  // Godot skips the hidden directory meanwhile.
  fs.mkdirSync(path.dirname(outputDirectory), {recursive: true});
  const temporaryRoot = fs.mkdtempSync(path.join(path.dirname(outputDirectory), `.${path.basename(outputDirectory)}-build-`));
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
      // Without a URL, Metro also inlines the whole map into the bundle.
      sourceMapUrl: path.basename(output) + '.map',
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
    const projectDependencies = dependencies.map(file => path.relative(projectRoot, file).replaceAll(path.sep, '/'));
    fs.renameSync(temporaryBundle, path.join(stagedOutput, bundleName));
    // A trailing comment keeps the source map's lines. The runtime checks that
    // a segment's base is the bundle it runs with.
    const mark = base ? `godotBase=${fingerprint(JSON.parse(fs.readFileSync(path.resolve(projectRoot, base) + '.dependencies.json', 'utf8')))}` : `godotBundle=${fingerprint(projectDependencies)}`;
    fs.appendFileSync(path.join(stagedOutput, bundleName), `\n//# ${mark}\n`);
    fs.renameSync(sourceMap, path.join(stagedOutput, bundleName + '.map'));
    fs.writeFileSync(path.join(stagedOutput, bundleName + '.dependencies.json'), JSON.stringify(projectDependencies, null, 2) + '\n');
    const manifest = stageAssets(result.assets ?? [], stagedOutput);
    fs.writeFileSync(path.join(stagedOutput, bundleName + '.assets-manifest.json'), JSON.stringify(manifest, null, 2) + '\n');
    replaceDirectory(stagedOutput, outputDirectory);
  } finally {
    fs.rmSync(temporaryRoot, {recursive: true, force: true});
  }
  return output;
}

// A segment's file sits beside the bundle it extends, named after its entry
// as Project Settings names it, without a script extension.
function segmentPath(bundlePath, entry) {
  const name = path.basename(entry).replace(/\.(js|jsx|ts|tsx)$/, '');
  return path.join(path.dirname(bundlePath), `${name}.segment.js`);
}

// Builds the project's bundle, then the segments it loads at startup.
async function buildProject(project, {entry = project.entry, out = project.bundlePath, dev = false, minify = false} = {}) {
  const output = await buildBundle({projectRoot: project.jsRoot, entry, out, dev, minify});
  for (const segment of project.segments) {
    await buildBundle({projectRoot: project.jsRoot, entry: segment, out: segmentPath(output, segment), dev, minify, base: output});
  }
  return output;
}

module.exports = {buildBundle, buildProject, findEntry, segmentConfig, segmentPath};
