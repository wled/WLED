const escapeRegExp = require('lodash.escaperegexp');
const capitalize = require('lodash.capitalize');
const isString = require('lodash.isstring');
const isPlainObject = require('lodash.isplainobject');
const uniqBy = require('lodash.uniqby');
const hostConfig = require('./lib/hosts-config');

const {hasOwnProperty} = Object.prototype;

/* eslint prefer-named-capture-group: "off" */

const FENCE_BLOCK_REGEXP = /^(([ \t]*`{3,4})([^\n]*)([\s\S]+?)(^[ \t]*\2))/gm;
const CODE_BLOCK_REGEXP = /(`(?!\\))((?:.(?!\1(?!\\)))*.?)\1/g;
const HTML_CODE_BLOCK_REGEXP = /(<code)+?((?!(<code|<\/code>)+?)[\S\s])*(<\/code>)+?/gim;
const LEADING_TRAILING_SLASH_REGEXP = /^\/?([^/]+(?:\/[^/]+)*)\/?$/;
const TRAILING_SLASH_REGEXP = /\/?$/;

function inverse(string) {
  return string
    .split('')
    .reverse()
    .join('');
}

function join(keywords) {
  return keywords
    .filter(Boolean)
    .map(escapeRegExp)
    .join('|');
}

function addLeadingAndTrailingSlash(value) {
  return value.replace(LEADING_TRAILING_SLASH_REGEXP, '/$1/');
}

function addTrailingSlash(value) {
  return value.replace(TRAILING_SLASH_REGEXP, '/');
}

function includesIgnoreCase(array, value) {
  return array.findIndex(arrayValue => arrayValue.toUpperCase() === value.toUpperCase()) > -1;
}

function buildMentionsRegexp({mentionsPrefixes}) {
  return `((?:(?:[^\\w\\n\\v\\r]|^)+(?:${join(mentionsPrefixes)})[\\w-\\.]+[^\\W])+)`;
}

function buildRefRegexp({actions, delimiters, issuePrefixes, issueURLSegments, hosts}) {
  return `(?:(?:[^\\w\\n\\v\\r]|^)+(${join(
    Object.keys(actions).flatMap(key => actions[key])
  )}))?(?:[^\\w\\n\\v\\r]|^|(?: |\\t)*(?:${join([' ', '\t', ...delimiters])})(?: |\\t)*)${
    hosts.length > 0 ? `(?:${join(hosts)})?` : ''
  }((?:(?:[\\w-\\.]+)\\/)+(?:[\\w-\\.]+))?(${join([...issuePrefixes, ...issueURLSegments])})(\\d+)(?!\\w)`;
}

function buildRegexp(options) {
  return new RegExp(
    options.mentionsPrefixes.length > 0
      ? `(?:${buildRefRegexp(options)}|${buildMentionsRegexp(options)})`
      : buildMentionsRegexp(options),
    'gim'
  );
}

function buildMentionRegexp({mentionsPrefixes}) {
  return new RegExp(`(${join(mentionsPrefixes)})([\\w-\\.]+)`, 'gim');
}

function parse(text, regexp, mentionRegexp, {actions, issuePrefixes, hosts}) {
  let parsed;
  const results = {
    actions: Object.keys(actions).reduce(
      (result, key) => (actions[key].length > 0 ? Object.assign(result, {[key]: []}) : result),
      {}
    ),
    refs: [],
    mentions: [],
  };
  let noCodeBlock = inverse(inverse(text.replace(FENCE_BLOCK_REGEXP, '')).replace(CODE_BLOCK_REGEXP, ''));

  while (regexp.test(noCodeBlock)) {
    noCodeBlock = noCodeBlock.replace(HTML_CODE_BLOCK_REGEXP, '');
  }

  while ((parsed = regexp.exec(noCodeBlock)) !== null) {
    let [raw, action, slug, prefix, issue, mentions] = parsed;
    prefix =
      prefix && issuePrefixes.some(issuePrefix => issuePrefix.toUpperCase() === prefix.toUpperCase())
        ? prefix
        : undefined;
    raw = parsed[0].slice(
      parsed[0].indexOf(
        parsed[1] || hosts.find(host => parsed[0].toUpperCase().includes(host.toUpperCase())) || parsed[2] || parsed[3]
      )
    );
    action = capitalize(parsed[1]);

    const actionTypes = Object.keys(actions).filter(key => includesIgnoreCase(actions[key], action));

    if (actionTypes.length > 0) {
      for (const actionType of actionTypes) {
        results.actions[actionType].push({raw, action, slug, prefix, issue});
      }
    } else if (issue) {
      results.refs.push({raw, slug, prefix, issue});
    } else if (mentions) {
      let parsedMention;
      while ((parsedMention = mentionRegexp.exec(mentions)) !== null) {
        const [rawMention, prefixMention, user] = parsedMention;

        results.mentions.push({raw: rawMention.trim(), prefix: prefixMention, user});
      }
    }
  }

  return results;
}

function typeError(parentOpt, opt) {
  return new TypeError(
    `The ${[parentOpt, opt].filter(Boolean).join('.')} property must be a String or an array of Strings`
  );
}

function normalize(options, parentOpt) {
  for (const opt of Object.keys(options)) {
    if (!parentOpt && opt === 'actions') {
      normalize(options[opt], opt);
    } else {
      if (!options[opt]) {
        options[opt] = [];
      } else if (isString(options[opt])) {
        options[opt] = [options[opt]];
      } else if (!Array.isArray(options[opt])) {
        throw typeError(parentOpt, opt);
      }

      if (options[opt].length !== 0 && !options[opt].every(opt => isString(opt))) {
        throw typeError(parentOpt, opt);
      }

      options[opt] = options[opt].filter(Boolean);
    }
  }
}

module.exports = (options = 'default', overrides = {}) => {
  if (!isString(options) && !isPlainObject(options)) {
    throw new TypeError('The options argument must be a String or an Object');
  }

  if (isPlainObject(options) && hasOwnProperty.call(options, 'actions') && !isPlainObject(options.actions)) {
    throw new TypeError('The options.actions property must be an Object');
  }

  if (isString(options) && !includesIgnoreCase(Object.keys(hostConfig), options)) {
    throw new TypeError(`The supported configuration are [${Object.keys(hostConfig).join(', ')}], got '${options}'`);
  }

  if (!isPlainObject(overrides)) {
    throw new TypeError('The overrides argument must be an Object');
  } else if (hasOwnProperty.call(overrides, 'actions') && !isPlainObject(overrides.actions)) {
    throw new TypeError('The overrides.actions property must be an Object');
  }

  options = isString(options) ? hostConfig[options.toLowerCase()] : options;

  const mergedOptions = {
    ...hostConfig.default,
    ...options,
    ...overrides,
    actions: {...hostConfig.default.actions, ...options.actions, ...overrides.actions},
  };

  normalize(mergedOptions);

  mergedOptions.hosts = mergedOptions.hosts.map(addTrailingSlash);
  mergedOptions.issueURLSegments = mergedOptions.issueURLSegments.map(addLeadingAndTrailingSlash);

  const regexp = buildRegexp(mergedOptions);
  const mentionRegexp = buildMentionRegexp(mergedOptions);

  return text => {
    if (!isString(text)) {
      throw new TypeError('The issue text must be a String');
    }

    const results = parse(text, regexp, mentionRegexp, mergedOptions);

    Reflect.defineProperty(results, 'allRefs', {
      get() {
        return uniqBy(this.refs.concat(...Object.keys(this.actions).map(key => this.actions[key])), 'raw');
      },
    });
    return results;
  };
};
