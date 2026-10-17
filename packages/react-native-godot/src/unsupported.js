'use strict';

export function unsupported(operation) {
  const error = new Error(operation + ' is not supported on the Godot platform.');
  error.code = 'E_UNSUPPORTED';
  error.operation = operation;
  throw error;
}
