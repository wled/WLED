import { inspect } from "util";
import { isString } from "lodash-es";
import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const { homepage } = require("../../package.json");

const stringify = (object) =>
  isString(object)
    ? object
    : inspect(object, {
        breakLength: Number.POSITIVE_INFINITY,
        depth: 2,
        maxArrayLength: 5,
      });
const linkify = (file) => `${homepage}/blob/master/${file}`;

export function EINVALIDCMD({ cmd, cmdProp }) {
  return {
    message: `Invalid \`${cmdProp}\` option.`,
    details: `The [\`${cmdProp}\` option](${linkify(
      `README.md#${cmdProp}`,
    )}) is required and must be a non empty \`String\`.

Your configuration for the \`${cmdProp}\` option is \`${stringify(cmd)}\`.`,
  };
}

export function EINVALIDSHELL({ shell }) {
  return {
    message: "Invalid `shell` option.",
    details: `The [\`shell\` option](${linkify(
      "README.md#options",
    )}) if defined, must be a non empty \`String\` or the value \`true\`.

Your configuration for the \`shell\` option is \`${stringify(shell)}\`.`,
  };
}

export function EINVALIDEXECCWD({ execCwd }) {
  return {
    message: "Invalid `execCwd` option.",
    details: `The [\`execCwd\` option](${linkify("README.md#options")}) if defined, must be a non empty \`String\`.

Your configuration for the \`execCwd\` option is \`${stringify(execCwd)}\`.`,
  };
}
