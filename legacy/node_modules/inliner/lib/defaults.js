module.exports = function () {
  return {
    images: true,
    compressCSS: true,
    compressJS: true,
    collapseWhitespace: true,
    nosvg: false, // by default, DO compress SVG with SVGO
    skipAbsoluteUrls: false,
    preserveComments: false,
    iesafe: false,
  };
};
