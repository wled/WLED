// https://docs.microsoft.com/en-us/azure/devops/pipelines/build/variables
// The docs indicate that SYSTEM_PULLREQUEST_SOURCEBRANCH and SYSTEM_PULLREQUEST_TARGETBRANCH are in the long format (e.g `refs/heads/master`) however tests show they are both in the short format (e.g. `master`)
import { parseBranch } from "../lib/utils.js";

export default {
  detect({ env }) {
    return Boolean(env.BUILD_BUILDURI);
  },
  configuration({ env }) {
    const pr = env.SYSTEM_PULLREQUEST_PULLREQUESTID;
    const isPr = Boolean(pr);

    return {
      name: "Azure Pipelines",
      service: "azurePipelines",
      commit: env.BUILD_SOURCEVERSION,
      build: env.BUILD_BUILDNUMBER,
      branch: parseBranch(
        isPr ? env.SYSTEM_PULLREQUEST_TARGETBRANCH : env.BUILD_SOURCEBRANCH,
      ),
      pr,
      isPr,
      prBranch: parseBranch(
        isPr ? env.SYSTEM_PULLREQUEST_SOURCEBRANCH : undefined,
      ),
      root: env.BUILD_REPOSITORY_LOCALPATH,
    };
  },
};
