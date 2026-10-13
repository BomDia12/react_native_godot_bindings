'use strict';

import './bootstrap-finalize';
import resolveAssetSource from 'react-native/Libraries/Image/resolveAssetSource';
import {godotAssetSource} from './asset-source';

resolveAssetSource.setCustomSourceTransformer(godotAssetSource);
