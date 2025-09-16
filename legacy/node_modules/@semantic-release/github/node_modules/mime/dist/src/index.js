import otherTypes from '../types/other.js';
import standardTypes from '../types/standard.js';
import Mime from './Mime.js';
export { default as Mime } from './Mime.js';
export default new Mime(standardTypes, otherTypes)._freeze();
