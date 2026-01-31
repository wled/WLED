#pragma once
/*
  asyncDNS.h - wrapper class for asynchronous DNS lookups using lwIP
  by @dedehai
*/

#ifndef ASYNC_DNS_H
#define ASYNC_DNS_H

#include <Arduino.h>
#include <lwip/dns.h>
#include <lwip/err.h>

enum class DnsResult { Idle, Busy, Success, Error };

class AsyncDNS {
private:
  ip_addr_t _raw_addr;
  volatile DnsResult _status = DnsResult::Idle;
  uint16_t _errorcount = 0;

  // callback for dns_gethostbyname(), called when lookup is complete or timed out
  static void _dns_callback(const char *name, const ip_addr_t *ipaddr, void *arg) {
    AsyncDNS* instance = static_cast<AsyncDNS*>(arg);
    if (ipaddr) {
      instance->_raw_addr = *ipaddr;
      instance->_status = DnsResult::Success;
    } else {
      instance->_status = DnsResult::Error; // note: if query timed out (~5s), DNS lookup is broken until WiFi connection is reset (IDF V4 bug)
      instance->_errorcount++;
    }
  }

public:
  // note: passing the IP as a pointer to query() is not implemented because it is not thread-safe without mutexes
  //       with the current IDF bug external error handlig is required anyway or dns will just stay stuck

  // non-blocking query function to start DNS lookup
  DnsResult query(const char* hostname) {
    if (_status == DnsResult::Busy) return DnsResult::Busy; // in progress, waiting for callback

    _status = DnsResult::Busy;
    err_t err = dns_gethostbyname(hostname, &_raw_addr, _dns_callback, this);

    if (err == ERR_OK) {
      _status = DnsResult::Success; // result already in cache
    } else if (err != ERR_INPROGRESS) {
      _status = DnsResult::Error;
      _errorcount++;
    }
    return _status;
  }

  // get the IP once Success is returned
  IPAddress getIP() {
    if (_status != DnsResult::Success) return IPAddress(0,0,0,0);
    #ifdef ARDUINO_ARCH_ESP32
      return IPAddress(_raw_addr.u_addr.ip4.addr);
    #else
      return IPAddress(_raw_addr.addr);
    #endif
  }

  void renew() { _status = DnsResult::Idle; } // reset status to allow re-query
  void reset() { _status = DnsResult::Idle; _errorcount = 0; } // reset status and error count
  DnsResult status() { return _status; }
  uint16_t getErrorCount() { return _errorcount; }
};
#endif // ASYNC_DNS_H