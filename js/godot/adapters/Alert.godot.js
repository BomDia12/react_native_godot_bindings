'use strict';
import {ambientAlert} from '../alerts';
export default class Alert {
  static alert(title, message, buttons, options) {
    ambientAlert(title, message, buttons, options).catch(error => console.warn(error));
  }
  static prompt() {}
}
