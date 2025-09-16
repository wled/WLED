module.exports = class SemanticReleaseError extends Error {
  constructor(message, code, details) {
    super(message);
    Error.captureStackTrace(this, this.constructor);
    this.name = 'SemanticReleaseError';
    this.code = code;
    this.details = details;
    this.semanticRelease = true;
  }
};
