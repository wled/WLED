# @semantic-release/error

Error type used by all [semantic-release](https://github.com/semantic-release/semantic-release) packages.

[![Build Status](https://github.com/semantic-release/error/workflows/Test/badge.svg)](https://github.com/semantic-release/error/actions?query=workflow%3ATest+branch%3Amaster)

Errors of type `SemanticReleaseError` or an inherited type will be considered by [semantic-release](https://github.com/semantic-release/semantic-release) as an expected exception case (no release to be done, running on a PR etc..). That indicate to the `semantic-release` process to stop and exit with the `0` success code.

Any other type of error will be considered by [semantic-release](https://github.com/semantic-release/semantic-release) as an unexpected error (i/o issue, code problem etc...). That indicate to the `semantic-release` process to stop, log the error and exit with the `1` failure code.

## Usage

```js
import SemanticReleaseError from "@semantic-release/error";

// Default
throw new SemanticReleaseError();

// With error message
throw new SemanticReleaseError("An error happened");

// With error message and error code
throw new SemanticReleaseError("An error happened", "ECODE");

// With error message, error code and details
throw new SemanticReleaseError("An error happened", "ECODE", "Here is some suggestions to solve this error.");

// With inheritance
class InheritedError extends SemanticReleaseError {
  constructor(message, code, newProperty, details) {
    super(message);
    Error.captureStackTrace(this, this.constructor);
    this.name = this.constructor.name;
    this.code = code;
    this.details = details;
    this.newProperty = "newProperty";
  }
}

throw new InheritedError("An error happened", "ECODE", "Here is some suggestions to solve this error.");
```
