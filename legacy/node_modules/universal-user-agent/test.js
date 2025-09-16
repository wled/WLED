import assert from "node:assert";

import { getUserAgent } from "./index.js";

assert(getUserAgent instanceof Function, "getUserAgent is a function");
assert.equal(typeof getUserAgent(), "string", "getUserAgent returns a string");

console.log("ok");
