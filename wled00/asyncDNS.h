#pragma once
/*
  asyncDNS.h - wrapper class for asynchronous DNS lookups using lwIP
  by @dedehai
*/

#include <Arduino.h>
#include <atomic>
#include <memory>
#include <lwip/dns.h>
#include <lwip/err.h>


class AsyncDNS {

  // C++14 shim
#if __cplusplus < 201402L
  // Really simple C++11 shim for non-array case; implementation from cppreference.com
  template<class T, class... Args>
  static std::unique_ptr<T>
  make_unique(Args&&... args)
  {
      return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
  }
#endif

  public:
  // note: passing the IP as a pointer to query() is not implemented because it is not thread-safe without mutexes
  //       with the IDF V4 bug external error handling is required anyway or dns can just stay stuck
  enum class result { Idle, Busy, Success, Error };

  // non-blocking query function to start DNS lookup
  static std::shared_ptr<AsyncDNS> query(const char* hostname, std::shared_ptr<AsyncDNS> current = {}) {    
    if (!current || (current->_status == result::Busy)) {
      current.reset(new AsyncDNS());
    }

#if __cplusplus >= 201402L
    using std::make_unique;
#endif

    std::unique_ptr<std::shared_ptr<AsyncDNS>> callback_state = make_unique<std::shared_ptr<AsyncDNS> >(current);
    if (!callback_state) return {};

    current->_status = result::Busy;
    err_t err = dns_gethostbyname(hostname, &current->_raw_addr, _dns_callback, callback_state.get());
    if (err == ERR_OK) {
      current->_status = result::Success; // result already in cache
    } else if (err == ERR_INPROGRESS) {
      callback_state.release(); // belongs to the callback now
    } else {
      Serial.printf("DNS fail: %d\n", err);
      current->_status = result::Error;
      current->_errorcount++;
    }
    return current;
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

  void reset() { _errorcount = 0; } // reset status and error count
  const result status() { return _status; }
  const uint16_t getErrorCount() { return _errorcount; }

  private:
  ip_addr_t _raw_addr;
  std::atomic<result> _status { result::Idle };
  uint16_t _errorcount = 0;

  AsyncDNS(){}; // may not be explicitly instantiated - use query()

  // callback for dns_gethostbyname(), called when lookup is complete or timed out
  static void _dns_callback(const char *name, const ip_addr_t *ipaddr, void *arg) {
    std::shared_ptr<AsyncDNS>* instance_ptr = reinterpret_cast<std::shared_ptr<AsyncDNS>*>(arg);
    AsyncDNS& instance = **instance_ptr;
    if (ipaddr) {
      instance._raw_addr = *ipaddr;
      instance._status = result::Success;
    } else {
      instance._status = result::Error; // note: if query timed out (~5s), DNS lookup is broken until WiFi connection is reset (IDF V4 bug)
      instance._errorcount++;
    }
    delete instance_ptr;
  }
};
