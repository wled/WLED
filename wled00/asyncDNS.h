#pragma once
/*
  asyncDNS.h - wrapper class for asynchronous DNS lookups using lwIP
  by @dedehai
*/

#include <Arduino.h>
#include <atomic>
#include <lwip/dns.h>
#include <lwip/err.h>


class AsyncDNS {
public:
  // note: passing the IP as a pointer to query() is not implemented because it is not thread-safe without mutexes
  //       with the IDF V4 bug external error handling is required anyway or dns can just stay stuck
  enum class result { Idle, Busy, Success, Error };

  // non-blocking query function to start DNS lookup
  result query(const char* hostname) {
    if (_status == result::Busy) return result::Busy; // in progress, waiting for callback

    _status = result::Busy;
    err_t err = dns_gethostbyname(hostname, &_raw_addr, _dns_callback, this);
    if (err == ERR_OK) {
      _status = result::Success; // result already in cache
    } else if (err != ERR_INPROGRESS) {
      _status = result::Error;
      _errorcount++;
    }
    return _status;
  }

  // get the IP once Success is returned
  const IPAddress getIP() {
    if (_status != result::Success) return IPAddress(0,0,0,0);
    #ifdef ARDUINO_ARCH_ESP32
      return IPAddress(_raw_addr.u_addr.ip4.addr);
    #else
      return IPAddress(_raw_addr.addr);
    #endif
  }

  void renew() { _status = result::Idle; } // reset status to allow re-query
  void reset() { _status = result::Idle; _errorcount = 0; } // reset status and error count
  const result status() { return _status; }
  const uint16_t getErrorCount() { return _errorcount; }

  private:
  ip_addr_t _raw_addr;
  std::atomic<result> _status { result::Idle };
  uint16_t _errorcount = 0;

  // callback for dns_gethostbyname(), called when lookup is complete or timed out
  static void _dns_callback(const char *name, const ip_addr_t *ipaddr, void *arg) {
    AsyncDNS* instance = reinterpret_cast<AsyncDNS*>(arg);
    if (ipaddr) {
      instance->_raw_addr = *ipaddr;
      instance->_status = result::Success;
    } else {
      instance->_status = result::Error; // note: if query timed out (~5s), DNS lookup is broken until WiFi connection is reset (IDF V4 bug)
      instance->_errorcount++;
    }
  }
};
