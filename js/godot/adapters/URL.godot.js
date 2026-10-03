'use strict';

import {URL as UpstreamURL, URLSearchParams} from 'react-native-godot/UpstreamURL';
import {binaryService, descriptor} from '../binary';
export {URLSearchParams};
export class URL extends UpstreamURL {
  static createObjectURL(blob) { return binaryService().createURL(descriptor(blob.data)); }
  static revokeObjectURL(url) { binaryService().revokeURL(String(url)); }
}
