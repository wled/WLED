import { head, branch } from "../lib/git.js";

export default {
  configuration(options) {
    return { commit: head(options), branch: branch(options) };
  },
};
