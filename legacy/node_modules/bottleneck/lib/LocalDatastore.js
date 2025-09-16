"use strict";

function asyncGeneratorStep(gen, resolve, reject, _next, _throw, key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { Promise.resolve(value).then(_next, _throw); } }

function _asyncToGenerator(fn) { return function () { var self = this, args = arguments; return new Promise(function (resolve, reject) { var gen = fn.apply(self, args); function _next(value) { asyncGeneratorStep(gen, resolve, reject, _next, _throw, "next", value); } function _throw(err) { asyncGeneratorStep(gen, resolve, reject, _next, _throw, "throw", err); } _next(undefined); }); }; }

var BottleneckError, LocalDatastore, parser;
parser = require("./parser");
BottleneckError = require("./BottleneckError");
LocalDatastore = class LocalDatastore {
  constructor(instance, storeOptions, storeInstanceOptions) {
    this.instance = instance;
    this.storeOptions = storeOptions;
    this.clientId = this.instance._randomIndex();
    parser.load(storeInstanceOptions, storeInstanceOptions, this);
    this._nextRequest = this._lastReservoirRefresh = this._lastReservoirIncrease = Date.now();
    this._running = 0;
    this._done = 0;
    this._unblockTime = 0;
    this.ready = this.Promise.resolve();
    this.clients = {};

    this._startHeartbeat();
  }

  _startHeartbeat() {
    var base;

    if (this.heartbeat == null && (this.storeOptions.reservoirRefreshInterval != null && this.storeOptions.reservoirRefreshAmount != null || this.storeOptions.reservoirIncreaseInterval != null && this.storeOptions.reservoirIncreaseAmount != null)) {
      return typeof (base = this.heartbeat = setInterval(() => {
        var amount, incr, maximum, now, reservoir;
        now = Date.now();

        if (this.storeOptions.reservoirRefreshInterval != null && now >= this._lastReservoirRefresh + this.storeOptions.reservoirRefreshInterval) {
          this._lastReservoirRefresh = now;
          this.storeOptions.reservoir = this.storeOptions.reservoirRefreshAmount;

          this.instance._drainAll(this.computeCapacity());
        }

        if (this.storeOptions.reservoirIncreaseInterval != null && now >= this._lastReservoirIncrease + this.storeOptions.reservoirIncreaseInterval) {
          var _this$storeOptions = this.storeOptions;
          amount = _this$storeOptions.reservoirIncreaseAmount;
          maximum = _this$storeOptions.reservoirIncreaseMaximum;
          reservoir = _this$storeOptions.reservoir;
          this._lastReservoirIncrease = now;
          incr = maximum != null ? Math.min(amount, maximum - reservoir) : amount;

          if (incr > 0) {
            this.storeOptions.reservoir += incr;
            return this.instance._drainAll(this.computeCapacity());
          }
        }
      }, this.heartbeatInterval)).unref === "function" ? base.unref() : void 0;
    } else {
      return clearInterval(this.heartbeat);
    }
  }

  __publish__(message) {
    var _this = this;

    return _asyncToGenerator(function* () {
      yield _this.yieldLoop();
      return _this.instance.Events.trigger("message", message.toString());
    })();
  }

  __disconnect__(flush) {
    var _this2 = this;

    return _asyncToGenerator(function* () {
      yield _this2.yieldLoop();
      clearInterval(_this2.heartbeat);
      return _this2.Promise.resolve();
    })();
  }

  yieldLoop(t = 0) {
    return new this.Promise(function (resolve, reject) {
      return setTimeout(resolve, t);
    });
  }

  computePenalty() {
    var ref;
    return (ref = this.storeOptions.penalty) != null ? ref : 15 * this.storeOptions.minTime || 5000;
  }

  __updateSettings__(options) {
    var _this3 = this;

    return _asyncToGenerator(function* () {
      yield _this3.yieldLoop();
      parser.overwrite(options, options, _this3.storeOptions);

      _this3._startHeartbeat();

      _this3.instance._drainAll(_this3.computeCapacity());

      return true;
    })();
  }

  __running__() {
    var _this4 = this;

    return _asyncToGenerator(function* () {
      yield _this4.yieldLoop();
      return _this4._running;
    })();
  }

  __queued__() {
    var _this5 = this;

    return _asyncToGenerator(function* () {
      yield _this5.yieldLoop();
      return _this5.instance.queued();
    })();
  }

  __done__() {
    var _this6 = this;

    return _asyncToGenerator(function* () {
      yield _this6.yieldLoop();
      return _this6._done;
    })();
  }

  __groupCheck__(time) {
    var _this7 = this;

    return _asyncToGenerator(function* () {
      yield _this7.yieldLoop();
      return _this7._nextRequest + _this7.timeout < time;
    })();
  }

  computeCapacity() {
    var maxConcurrent, reservoir;
    var _this$storeOptions2 = this.storeOptions;
    maxConcurrent = _this$storeOptions2.maxConcurrent;
    reservoir = _this$storeOptions2.reservoir;

    if (maxConcurrent != null && reservoir != null) {
      return Math.min(maxConcurrent - this._running, reservoir);
    } else if (maxConcurrent != null) {
      return maxConcurrent - this._running;
    } else if (reservoir != null) {
      return reservoir;
    } else {
      return null;
    }
  }

  conditionsCheck(weight) {
    var capacity;
    capacity = this.computeCapacity();
    return capacity == null || weight <= capacity;
  }

  __incrementReservoir__(incr) {
    var _this8 = this;

    return _asyncToGenerator(function* () {
      var reservoir;
      yield _this8.yieldLoop();
      reservoir = _this8.storeOptions.reservoir += incr;

      _this8.instance._drainAll(_this8.computeCapacity());

      return reservoir;
    })();
  }

  __currentReservoir__() {
    var _this9 = this;

    return _asyncToGenerator(function* () {
      yield _this9.yieldLoop();
      return _this9.storeOptions.reservoir;
    })();
  }

  isBlocked(now) {
    return this._unblockTime >= now;
  }

  check(weight, now) {
    return this.conditionsCheck(weight) && this._nextRequest - now <= 0;
  }

  __check__(weight) {
    var _this10 = this;

    return _asyncToGenerator(function* () {
      var now;
      yield _this10.yieldLoop();
      now = Date.now();
      return _this10.check(weight, now);
    })();
  }

  __register__(index, weight, expiration) {
    var _this11 = this;

    return _asyncToGenerator(function* () {
      var now, wait;
      yield _this11.yieldLoop();
      now = Date.now();

      if (_this11.conditionsCheck(weight)) {
        _this11._running += weight;

        if (_this11.storeOptions.reservoir != null) {
          _this11.storeOptions.reservoir -= weight;
        }

        wait = Math.max(_this11._nextRequest - now, 0);
        _this11._nextRequest = now + wait + _this11.storeOptions.minTime;
        return {
          success: true,
          wait,
          reservoir: _this11.storeOptions.reservoir
        };
      } else {
        return {
          success: false
        };
      }
    })();
  }

  strategyIsBlock() {
    return this.storeOptions.strategy === 3;
  }

  __submit__(queueLength, weight) {
    var _this12 = this;

    return _asyncToGenerator(function* () {
      var blocked, now, reachedHWM;
      yield _this12.yieldLoop();

      if (_this12.storeOptions.maxConcurrent != null && weight > _this12.storeOptions.maxConcurrent) {
        throw new BottleneckError(`Impossible to add a job having a weight of ${weight} to a limiter having a maxConcurrent setting of ${_this12.storeOptions.maxConcurrent}`);
      }

      now = Date.now();
      reachedHWM = _this12.storeOptions.highWater != null && queueLength === _this12.storeOptions.highWater && !_this12.check(weight, now);
      blocked = _this12.strategyIsBlock() && (reachedHWM || _this12.isBlocked(now));

      if (blocked) {
        _this12._unblockTime = now + _this12.computePenalty();
        _this12._nextRequest = _this12._unblockTime + _this12.storeOptions.minTime;

        _this12.instance._dropAllQueued();
      }

      return {
        reachedHWM,
        blocked,
        strategy: _this12.storeOptions.strategy
      };
    })();
  }

  __free__(index, weight) {
    var _this13 = this;

    return _asyncToGenerator(function* () {
      yield _this13.yieldLoop();
      _this13._running -= weight;
      _this13._done += weight;

      _this13.instance._drainAll(_this13.computeCapacity());

      return {
        running: _this13._running
      };
    })();
  }

};
module.exports = LocalDatastore;