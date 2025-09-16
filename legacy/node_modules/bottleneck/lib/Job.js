"use strict";

function asyncGeneratorStep(gen, resolve, reject, _next, _throw, key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { Promise.resolve(value).then(_next, _throw); } }

function _asyncToGenerator(fn) { return function () { var self = this, args = arguments; return new Promise(function (resolve, reject) { var gen = fn.apply(self, args); function _next(value) { asyncGeneratorStep(gen, resolve, reject, _next, _throw, "next", value); } function _throw(err) { asyncGeneratorStep(gen, resolve, reject, _next, _throw, "throw", err); } _next(undefined); }); }; }

var BottleneckError, DEFAULT_PRIORITY, Job, NUM_PRIORITIES, parser;
NUM_PRIORITIES = 10;
DEFAULT_PRIORITY = 5;
parser = require("./parser");
BottleneckError = require("./BottleneckError");
Job = class Job {
  constructor(task, args, options, jobDefaults, rejectOnDrop, Events, _states, Promise) {
    this.task = task;
    this.args = args;
    this.rejectOnDrop = rejectOnDrop;
    this.Events = Events;
    this._states = _states;
    this.Promise = Promise;
    this.options = parser.load(options, jobDefaults);
    this.options.priority = this._sanitizePriority(this.options.priority);

    if (this.options.id === jobDefaults.id) {
      this.options.id = `${this.options.id}-${this._randomIndex()}`;
    }

    this.promise = new this.Promise((_resolve, _reject) => {
      this._resolve = _resolve;
      this._reject = _reject;
    });
    this.retryCount = 0;
  }

  _sanitizePriority(priority) {
    var sProperty;
    sProperty = ~~priority !== priority ? DEFAULT_PRIORITY : priority;

    if (sProperty < 0) {
      return 0;
    } else if (sProperty > NUM_PRIORITIES - 1) {
      return NUM_PRIORITIES - 1;
    } else {
      return sProperty;
    }
  }

  _randomIndex() {
    return Math.random().toString(36).slice(2);
  }

  doDrop({
    error,
    message = "This job has been dropped by Bottleneck"
  } = {}) {
    if (this._states.remove(this.options.id)) {
      if (this.rejectOnDrop) {
        this._reject(error != null ? error : new BottleneckError(message));
      }

      this.Events.trigger("dropped", {
        args: this.args,
        options: this.options,
        task: this.task,
        promise: this.promise
      });
      return true;
    } else {
      return false;
    }
  }

  _assertStatus(expected) {
    var status;
    status = this._states.jobStatus(this.options.id);

    if (!(status === expected || expected === "DONE" && status === null)) {
      throw new BottleneckError(`Invalid job status ${status}, expected ${expected}. Please open an issue at https://github.com/SGrondin/bottleneck/issues`);
    }
  }

  doReceive() {
    this._states.start(this.options.id);

    return this.Events.trigger("received", {
      args: this.args,
      options: this.options
    });
  }

  doQueue(reachedHWM, blocked) {
    this._assertStatus("RECEIVED");

    this._states.next(this.options.id);

    return this.Events.trigger("queued", {
      args: this.args,
      options: this.options,
      reachedHWM,
      blocked
    });
  }

  doRun() {
    if (this.retryCount === 0) {
      this._assertStatus("QUEUED");

      this._states.next(this.options.id);
    } else {
      this._assertStatus("EXECUTING");
    }

    return this.Events.trigger("scheduled", {
      args: this.args,
      options: this.options
    });
  }

  doExecute(chained, clearGlobalState, run, free) {
    var _this = this;

    return _asyncToGenerator(function* () {
      var error, eventInfo, passed;

      if (_this.retryCount === 0) {
        _this._assertStatus("RUNNING");

        _this._states.next(_this.options.id);
      } else {
        _this._assertStatus("EXECUTING");
      }

      eventInfo = {
        args: _this.args,
        options: _this.options,
        retryCount: _this.retryCount
      };

      _this.Events.trigger("executing", eventInfo);

      try {
        passed = yield chained != null ? chained.schedule(_this.options, _this.task, ..._this.args) : _this.task(..._this.args);

        if (clearGlobalState()) {
          _this.doDone(eventInfo);

          yield free(_this.options, eventInfo);

          _this._assertStatus("DONE");

          return _this._resolve(passed);
        }
      } catch (error1) {
        error = error1;
        return _this._onFailure(error, eventInfo, clearGlobalState, run, free);
      }
    })();
  }

  doExpire(clearGlobalState, run, free) {
    var error, eventInfo;

    if (this._states.jobStatus(this.options.id === "RUNNING")) {
      this._states.next(this.options.id);
    }

    this._assertStatus("EXECUTING");

    eventInfo = {
      args: this.args,
      options: this.options,
      retryCount: this.retryCount
    };
    error = new BottleneckError(`This job timed out after ${this.options.expiration} ms.`);
    return this._onFailure(error, eventInfo, clearGlobalState, run, free);
  }

  _onFailure(error, eventInfo, clearGlobalState, run, free) {
    var _this2 = this;

    return _asyncToGenerator(function* () {
      var retry, retryAfter;

      if (clearGlobalState()) {
        retry = yield _this2.Events.trigger("failed", error, eventInfo);

        if (retry != null) {
          retryAfter = ~~retry;

          _this2.Events.trigger("retry", `Retrying ${_this2.options.id} after ${retryAfter} ms`, eventInfo);

          _this2.retryCount++;
          return run(retryAfter);
        } else {
          _this2.doDone(eventInfo);

          yield free(_this2.options, eventInfo);

          _this2._assertStatus("DONE");

          return _this2._reject(error);
        }
      }
    })();
  }

  doDone(eventInfo) {
    this._assertStatus("EXECUTING");

    this._states.next(this.options.id);

    return this.Events.trigger("done", eventInfo);
  }

};
module.exports = Job;