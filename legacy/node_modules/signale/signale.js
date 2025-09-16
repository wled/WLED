'use strict';
const util = require('util');
const path = require('path');
const readline = require('readline');
const chalk = require('chalk');
const figures = require('figures');
const pkgConf = require('pkg-conf');
const pkg = require('./package.json');
const defaultTypes = require('./types');

const {green, grey, red, underline, yellow} = chalk;

let isPreviousLogInteractive = false;
const defaults = pkg.options.default;
const namespace = pkg.name;

class Signale {
  constructor(options = {}) {
    this._interactive = options.interactive || false;
    this._config = Object.assign(this.packageConfiguration, options.config);
    this._customTypes = Object.assign({}, options.types);
    this._disabled = options.disabled || false;
    this._scopeName = options.scope || '';
    this._timers = options.timers || new Map();
    this._types = this._mergeTypes(defaultTypes, this._customTypes);
    this._stream = options.stream || process.stdout;
    this._longestLabel = this._getLongestLabel();
    this._secrets = options.secrets || [];
    this._generalLogLevel = this._validateLogLevel(options.logLevel);

    Object.keys(this._types).forEach(type => {
      this[type] = this._logger.bind(this, type);
    });
  }

  get _now() {
    return Date.now();
  }

  get scopeName() {
    return this._scopeName;
  }

  get currentOptions() {
    return Object.assign({}, {
      config: this._config,
      disabled: this._disabled,
      types: this._customTypes,
      interactive: this._interactive,
      timers: this._timers,
      stream: this._stream,
      secrets: this._secrets,
      logLevel: this._generalLogLevel
    });
  }

  get date() {
    return new Date().toLocaleDateString();
  }

  get timestamp() {
    return new Date().toLocaleTimeString();
  }

  get filename() {
    const _ = Error.prepareStackTrace;
    Error.prepareStackTrace = (error, stack) => stack;
    const {stack} = new Error();
    Error.prepareStackTrace = _;

    const callers = stack.map(x => x.getFileName());

    const firstExternalFilePath = callers.find(x => {
      return x !== callers[0];
    });

    return firstExternalFilePath ? path.basename(firstExternalFilePath) : 'anonymous';
  }

  get packageConfiguration() {
    return pkgConf.sync(namespace, {defaults});
  }

  get _longestUnderlinedLabel() {
    return underline(this._longestLabel);
  }

  get _logLevels() {
    return {
      info: 0,
      timer: 1,
      debug: 2,
      warn: 3,
      error: 4
    };
  }

  set configuration(configObj) {
    this._config = Object.assign(this.packageConfiguration, configObj);
  }

  _arrayify(x) {
    return Array.isArray(x) ? x : [x];
  }

  _timeSpan(then) {
    return (this._now - then);
  }

  _getLongestLabel() {
    const {_types} = this;
    const labels = Object.keys(_types).map(x => _types[x].label);
    return labels.reduce((x, y) => x.length > y.length ? x : y);
  }

  _validateLogLevel(level) {
    return Object.keys(this._logLevels).includes(level) ? level : 'info';
  }

  _mergeTypes(standard, custom) {
    const types = Object.assign({}, standard);

    Object.keys(custom).forEach(type => {
      types[type] = Object.assign({}, types[type], custom[type]);
    });

    return types;
  }

  _filterSecrets(message) {
    const {_secrets} = this;

    if (_secrets.length === 0) {
      return message;
    }

    let safeMessage = message;

    _secrets.forEach(secret => {
      safeMessage = safeMessage.replace(new RegExp(secret, 'g'), '[secure]');
    });

    return safeMessage;
  }

  _formatStream(stream) {
    return this._arrayify(stream);
  }

  _formatDate() {
    return `[${this.date}]`;
  }

  _formatFilename() {
    return `[${this.filename}]`;
  }

  _formatScopeName() {
    if (Array.isArray(this._scopeName)) {
      const scopes = this._scopeName.filter(x => x.length !== 0);
      return `${scopes.map(x => `[${x.trim()}]`).join(' ')}`;
    }

    return `[${this._scopeName}]`;
  }

  _formatTimestamp() {
    return `[${this.timestamp}]`;
  }

  _formatMessage(str) {
    return util.format(...this._arrayify(str));
  }

  _meta() {
    const meta = [];

    if (this._config.displayDate) {
      meta.push(this._formatDate());
    }

    if (this._config.displayTimestamp) {
      meta.push(this._formatTimestamp());
    }

    if (this._config.displayFilename) {
      meta.push(this._formatFilename());
    }

    if (this._scopeName.length !== 0 && this._config.displayScope) {
      meta.push(this._formatScopeName());
    }

    if (meta.length !== 0) {
      meta.push(`${figures.pointerSmall}`);
      return meta.map(item => grey(item));
    }

    return meta;
  }

  _hasAdditional({suffix, prefix}, args) {
    return (suffix || prefix) ? '' : this._formatMessage(args);
  }

  _buildSignale(type, ...args) {
    let [msg, additional] = [{}, {}];

    if (args.length === 1 && typeof (args[0]) === 'object' && args[0] !== null) {
      if (args[0] instanceof Error) {
        [msg] = args;
      } else {
        const [{prefix, message, suffix}] = args;
        additional = Object.assign({}, {suffix, prefix});
        msg = message ? this._formatMessage(message) : this._hasAdditional(additional, args);
      }
    } else {
      msg = this._formatMessage(args);
    }

    const signale = this._meta();

    if (additional.prefix) {
      if (this._config.underlinePrefix) {
        signale.push(underline(additional.prefix));
      } else {
        signale.push(additional.prefix);
      }
    }

    if (this._config.displayBadge && type.badge) {
      signale.push(chalk[type.color](this._padEnd(type.badge, type.badge.length + 1)));
    }

    if (this._config.displayLabel && type.label) {
      const label = this._config.uppercaseLabel ? type.label.toUpperCase() : type.label;
      if (this._config.underlineLabel) {
        signale.push(chalk[type.color](this._padEnd(underline(label), this._longestUnderlinedLabel.length + 1)));
      } else {
        signale.push(chalk[type.color](this._padEnd(label, this._longestLabel.length + 1)));
      }
    }

    if (msg instanceof Error && msg.stack) {
      const [name, ...rest] = msg.stack.split('\n');
      if (this._config.underlineMessage) {
        signale.push(underline(name));
      } else {
        signale.push(name);
      }

      signale.push(grey(rest.map(l => l.replace(/^/, '\n')).join('')));
      return signale.join(' ');
    }

    if (this._config.underlineMessage) {
      signale.push(underline(msg));
    } else {
      signale.push(msg);
    }

    if (additional.suffix) {
      if (this._config.underlineSuffix) {
        signale.push(underline(additional.suffix));
      } else {
        signale.push(additional.suffix);
      }
    }

    return signale.join(' ');
  }

  _write(stream, message) {
    if (this._interactive && stream.isTTY && isPreviousLogInteractive) {
      readline.moveCursor(stream, 0, -1);
      readline.clearLine(stream);
      readline.cursorTo(stream, 0);
    }

    stream.write(message + '\n');
    isPreviousLogInteractive = this._interactive;
  }

  _log(message, streams = this._stream, logLevel) {
    if (this.isEnabled() && this._logLevels[logLevel] >= this._logLevels[this._generalLogLevel]) {
      this._formatStream(streams).forEach(stream => {
        this._write(stream, message);
      });
    }
  }

  _logger(type, ...messageObj) {
    const {stream, logLevel} = this._types[type];
    const message = this._buildSignale(this._types[type], ...messageObj);
    this._log(this._filterSecrets(message), stream, this._validateLogLevel(logLevel));
  }

  _padEnd(str, targetLength) {
    str = String(str);
    targetLength = parseInt(targetLength, 10) || 0;

    if (str.length >= targetLength) {
      return str;
    }

    if (String.prototype.padEnd) {
      return str.padEnd(targetLength);
    }

    targetLength -= str.length;
    return str + ' '.repeat(targetLength);
  }

  addSecrets(secrets) {
    if (!Array.isArray(secrets)) {
      throw new TypeError('Argument must be an array.');
    }

    this._secrets.push(...secrets);
  }

  clearSecrets() {
    this._secrets = [];
  }

  config(configObj) {
    this.configuration = configObj;
  }

  disable() {
    this._disabled = true;
  }

  enable() {
    this._disabled = false;
  }

  isEnabled() {
    return !this._disabled;
  }

  scope(...name) {
    if (name.length === 0) {
      throw new Error('No scope name was defined.');
    }

    return new Signale(Object.assign(this.currentOptions, {scope: name}));
  }

  unscope() {
    this._scopeName = '';
  }

  time(label) {
    if (!label) {
      label = `timer_${this._timers.size}`;
    }

    this._timers.set(label, this._now);

    const message = this._meta();
    message.push(green(this._padEnd(this._types.start.badge, 2)));

    if (this._config.underlineLabel) {
      message.push(green(this._padEnd(underline(label), this._longestUnderlinedLabel.length + 1)));
    } else {
      message.push(green(this._padEnd(label, this._longestLabel.length + 1)));
    }

    message.push('Initialized timer...');
    this._log(message.join(' '), this._stream, 'timer');

    return label;
  }

  timeEnd(label) {
    if (!label && this._timers.size) {
      const is = x => x.includes('timer_');
      label = [...this._timers.keys()].reduceRight((x, y) => {
        return is(x) ? x : (is(y) ? y : null);
      });
    }

    if (this._timers.has(label)) {
      const span = this._timeSpan(this._timers.get(label));
      this._timers.delete(label);

      const message = this._meta();
      message.push(red(this._padEnd(this._types.pause.badge, 2)));

      if (this._config.underlineLabel) {
        message.push(red(this._padEnd(underline(label), this._longestUnderlinedLabel.length + 1)));
      } else {
        message.push(red(this._padEnd(label, this._longestLabel.length + 1)));
      }

      message.push('Timer run for:');
      message.push(yellow(span < 1000 ? span + 'ms' : (span / 1000).toFixed(2) + 's'));
      this._log(message.join(' '), this._stream, 'timer');

      return {label, span};
    }
  }
}

module.exports = Signale;
