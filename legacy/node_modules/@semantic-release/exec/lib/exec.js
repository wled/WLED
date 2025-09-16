import { resolve } from "path";
import { template } from "lodash-es";
import { execa } from "execa";

export default async function exec(
  cmdProp,
  { shell, execCwd, ...config },
  { cwd, env, stdout, stderr, logger, ...context },
) {
  const cmd = config[cmdProp] ? cmdProp : "cmd";
  const script = template(config[cmd])({ config, ...context });

  logger.log("Call script %s", script);

  const result = execa(script, {
    shell: shell || true,
    cwd: execCwd ? resolve(cwd, execCwd) : cwd,
    env,
  });

  result.stdout.pipe(stdout, { end: false });
  result.stderr.pipe(stderr, { end: false });

  return (await result).stdout.trim();
}
