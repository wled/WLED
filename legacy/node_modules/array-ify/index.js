'use strict';
module.exports = function(val) {
  return Array.isArray(val) ? val : [val];
};
