import path from "path";
import rc from "rc";
import getRegistryUrl from "registry-auth-token/registry-url.js";

export default function ({ publishConfig: { registry } = {}, name }, { cwd, env }) {
  return (
    registry ||
    env.NPM_CONFIG_REGISTRY ||
    getRegistryUrl(
      name.split("/")[0],
      rc(
        "npm",
        { registry: "https://registry.npmjs.org/" },
        { config: env.NPM_CONFIG_USERCONFIG || path.resolve(cwd, ".npmrc") }
      )
    )
  );
}
