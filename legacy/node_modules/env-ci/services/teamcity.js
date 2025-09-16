// https://confluence.jetbrains.com/display/TCD10/Predefined+Build+Parameters

import javaProperties from "java-properties";

import { branch } from "../lib/git.js";

const PROPERTIES_MAPPING = {
  root: "teamcity.build.workingDir",
  branch: "teamcity.build.branch",
};

const safeReadProperties = (filePath) => {
  try {
    return javaProperties.of(filePath);
  } catch {
    return undefined;
  }
};

const getProperties = ({ env, cwd }) => {
  const buildProperties = env.TEAMCITY_BUILD_PROPERTIES_FILE
    ? safeReadProperties(env.TEAMCITY_BUILD_PROPERTIES_FILE)
    : undefined;
  const configFile = buildProperties
    ? buildProperties.get("teamcity.configuration.properties.file")
    : undefined;
  const configProperties = configFile
    ? safeReadProperties(configFile)
    : configFile;

  return Object.fromEntries(
    Object.keys(PROPERTIES_MAPPING).map((key) => [
      key,
      (buildProperties
        ? buildProperties.get(PROPERTIES_MAPPING[key])
        : undefined) ||
        (configProperties
          ? configProperties.get(PROPERTIES_MAPPING[key])
          : undefined) ||
        (key === "branch" ? branch({ env, cwd }) : undefined),
    ]),
  );
};

export default {
  detect({ env }) {
    return Boolean(env.TEAMCITY_VERSION);
  },
  configuration({ env, cwd }) {
    return {
      name: "TeamCity",
      service: "teamcity",
      commit: env.BUILD_VCS_NUMBER,
      build: env.BUILD_NUMBER,
      slug: env.TEAMCITY_BUILDCONF_NAME,
      ...getProperties({ env, cwd }),
    };
  },
};
