'use strict';

const fs = require('fs');
const path = require('path');
const {spawnSync} = require('child_process');

// Regenerates the TypeScript for the project's component contracts, which
// only the Godot editor can read.
function generateContracts(project, editor) {
  if (!fs.existsSync(path.join(project.jsRoot, 'contracts'))) {
    return;
  }
  const result = spawnSync(editor, ['--headless', '--editor', '--path', project.godotRoot, '--react-native-generate-contracts'], {stdio: 'inherit'});
  if (result.status !== 0) {
    throw new Error(`Generating the component contracts failed: ${result.error?.message ?? `exit code ${result.status ?? result.signal}`}`);
  }
}

module.exports = {generateContracts};
