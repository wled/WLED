
#pragma once

#ifndef WLED_DISABLE_ESPNOW_NEW

//#include "const.h"

#ifndef WLED_ESPNOW_MAX_QUEUED_MESSAGES
#define WLED_ESPNOW_MAX_QUEUED_MESSAGES 6
#endif

#ifndef WLED_ESPNOW_MAX_MESSAGE_LENGTH
#define WLED_ESPNOW_MAX_MESSAGE_LENGTH 250
#endif

#ifndef WLED_ESPNOW_MAX_REGISTERED_CALLBACKS
  #ifndef WLED_MAX_USERMODS
  #define WLED_MAX_USERMODS 1
  #endif
#define WLED_ESPNOW_MAX_REGISTERED_CALLBACKS WLED_MAX_USERMODS+1
#endif

class ESPNOWBroadcast {

  public:

    bool setup();

    void loop(size_t maxMessagesToProcess = WLED_ESPNOW_MAX_QUEUED_MESSAGES);

    bool send(const uint8_t* msg, size_t len);

    typedef void (*receive_callback_t)(const uint8_t *sender, const uint8_t *data, uint8_t len, int8_t rssi);
    bool registerCallback( receive_callback_t callback );
    bool removeCallback( receive_callback_t callback );

    // the receive filter happens from the WiFi task and this should not do any lengthly processing
    // additionally thread synchronization may need to be done with the main application task thread
    // only a single filter may be register at a given time
    // The filter should return false if the received network message should be ignored and not
    // processed by future callbacks
    typedef bool (*receive_filter_t)(const uint8_t *sender, const uint8_t *data, uint8_t len, int8_t rssi);
    receive_filter_t registerFilter( receive_filter_t filter = nullptr );


    enum STATE {
        STOPPED = 0,
        STARTING,
        STARTED,
        MAX
    };

    STATE getState() const;

    bool isStarted() const {
      return STATE::STARTED == getState();
    }

  protected:
    receive_callback_t _rxCallbacks[WLED_ESPNOW_MAX_REGISTERED_CALLBACKS] = {0};
    static constexpr size_t _rxCallbacksSize = sizeof(_rxCallbacks)/sizeof(_rxCallbacks[0]);
};

extern ESPNOWBroadcast espnowBroadcast;
#endif