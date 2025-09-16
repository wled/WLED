import appveyor from "./services/appveyor.js";
import azurePipelines from "./services/azure-pipelines.js";
import bamboo from "./services/bamboo.js";
import bitbucket from "./services/bitbucket.js";
import bitrise from "./services/bitrise.js";
import buddy from "./services/buddy.js";
import buildkite from "./services/buildkite.js";
import circleci from "./services/circleci.js";
import cirrus from "./services/cirrus.js";
import cloudflarePages from "./services/cloudflare-pages.js";
import codebuild from "./services/codebuild.js";
import codefresh from "./services/codefresh.js";
import codeship from "./services/codeship.js";
import drone from "./services/drone.js";
import git from "./services/git.js";
import github from "./services/github.js";
import gitlab from "./services/gitlab.js";
import jenkins from "./services/jenkins.js";
import netlify from "./services/netlify.js";
import puppet from "./services/puppet.js";
import sail from "./services/sail.js";
import screwdriver from "./services/screwdriver.js";
import scrutinizer from "./services/scrutinizer.js";
import semaphore from "./services/semaphore.js";
import shippable from "./services/shippable.js";
import teamcity from "./services/teamcity.js";
import travis from "./services/travis.js";
import vela from "./services/vela.js";
import vercel from "./services/vercel.js";
import wercker from "./services/wercker.js";
import woodpecker from "./services/woodpecker.js";
import jetbrainsSpace from "./services/jetbrains-space.js";

const services = {
  appveyor,
  azurePipelines,
  bamboo,
  bitbucket,
  bitrise,
  buddy,
  buildkite,
  circleci,
  cirrus,
  cloudflarePages,
  codebuild,
  codefresh,
  codeship,
  drone,
  github,
  gitlab,
  jenkins,
  netlify,
  puppet,
  sail,
  screwdriver,
  scrutinizer,
  semaphore,
  shippable,
  teamcity,
  travis,
  vela,
  vercel,
  wercker,
  woodpecker,
  jetbrainsSpace,
};

export default ({ env = process.env, cwd = process.cwd() } = {}) => {
  for (const name of Object.keys(services)) {
    if (services[name].detect({ env, cwd })) {
      return { isCi: true, ...services[name].configuration({ env, cwd }) };
    }
  }

  return { isCi: Boolean(env.CI), ...git.configuration({ env, cwd }) };
};
