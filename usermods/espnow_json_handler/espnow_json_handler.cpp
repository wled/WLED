#include "wled.h"

#ifndef WLED_DISABLE_ESPNOW

/*
 * ESP-NOW JSON Handler Usermod
 *
 * This usermod handles fragmented JSON messages received via ESP-NOW.
 * It reassembles fragments and deserializes the complete JSON payload
 * to control WLED state.
 *
 * Fragment header structure (3 bytes):
 *   - Byte 0: message_id (unique identifier for reassembly)
 *   - Byte 1: fragment_index (0-based fragment number)
 *   - Byte 2: total_fragments (total number of fragments in message)
 *   - Byte 3+: JSON data payload
 */

#define ESPNOW_FRAGMENT_HEADER_SIZE 3
#define ESPNOW_REASSEMBLY_TIMEOUT_MS 5000  // 5 second timeout for incomplete fragment reassembly

class EspNowJsonHandler : public Usermod {

private:
  // Fragment reassembly state
    uint8_t lastMsgId = 0;
    uint8_t lastProcessedMsgId = 0;
    uint8_t fragmentsReceived = 0;
    uint8_t* reassemblyBuffer = nullptr;
    size_t reassemblySize = 0;
    unsigned long reassemblyStartTime = 0;  // Timestamp when reassembly began

    // Cleanup reassembly state
    void cleanupReassembly() {
        if (reassemblyBuffer) {
            free(reassemblyBuffer);
            reassemblyBuffer = nullptr;
        }
        fragmentsReceived = 0;
        reassemblySize = 0;
        reassemblyStartTime = 0;
    }

public:
    void setup() override {
      // Nothing to initialize
    }

    void loop() override {
        // Check for stale reassembly state and clean up if timed out
        if (reassemblyBuffer && reassemblyStartTime > 0) {
            if (millis() - reassemblyStartTime > ESPNOW_REASSEMBLY_TIMEOUT_MS) {
                DEBUG_PRINTF_P(PSTR("ESP-NOW reassembly timeout for message %d, discarding %d fragments\n"), lastMsgId, fragmentsReceived);
                cleanupReassembly();
            }
        }
    }

    /**
     * Handle incoming ESP-NOW messages
     * Returns true if the message was handled (prevents default processing)
     */
    bool onEspNowMessage(uint8_t* sender, uint8_t* payload, uint8_t len) override {
    
        bool knownRemote = false;
        for (const auto& mac : linked_remotes) {
            if (strlen(mac.data()) == 12 && strcmp(last_signal_src, mac.data()) == 0) {
                knownRemote = true;
                break;
            }
        }
        if (!knownRemote) {
            DEBUG_PRINT(F("ESP Now Message Received from Unlinked Sender: "));
            DEBUG_PRINTLN(last_signal_src);
            return false; // Not handled
        }

      // Need at least header size to process
        if (len < ESPNOW_FRAGMENT_HEADER_SIZE) {
            return false; // Not handled
        }

        // Check if this looks like a fragmented JSON message
        // First byte should be a reasonable message ID (not a WiZ Mote signature or 'W' sync packet)
        if (payload[0] == 0x91 || payload[0] == 0x81 || payload[0] == 0x80 || payload[0] == 'W') {
            return false; // Let default handlers process these
        }

        uint8_t messageId = payload[0];
        uint8_t fragmentIndex = payload[1];
        uint8_t totalFragments = payload[2];

        // Validate fragment header - sanity checks
        if (totalFragments == 0 || fragmentIndex >= totalFragments) {
            return false; // Invalid fragment header
        }

        DEBUG_PRINTF_P(PSTR("ESP-NOW JSON fragment %d/%d of message %d (%d bytes)\n"),
            fragmentIndex + 1, totalFragments, messageId, len - ESPNOW_FRAGMENT_HEADER_SIZE);

        // Check if this message was already processed (deduplication for multi-channel reception)
        if (messageId == lastProcessedMsgId) {
            DEBUG_PRINTF_P(PSTR("ESP-NOW message %d already processed, skipping\n"), messageId);
            // If we're currently reassembling this message, clean up
            if (messageId == lastMsgId && reassemblyBuffer) {
                cleanupReassembly();
            }
            return true; // Message was handled (by ignoring duplicate)
        }

        // Check if this is a new message
        if (messageId != lastMsgId) {
          // Clean up old reassembly buffer if exists
            cleanupReassembly();
            lastMsgId = messageId;
            reassemblyStartTime = millis();  // Start timeout timer for new message
        }

        // Validate fragment index is sequential
        if (fragmentIndex != fragmentsReceived) {
            DEBUG_PRINTF_P(PSTR("ESP-NOW fragment out of order: expected %d, got %d\n"),
                fragmentsReceived, fragmentIndex);
            cleanupReassembly();
            return true; // Handled by aborting
        }

        // Allocate or reallocate buffer
        size_t fragmentDataSize = len - ESPNOW_FRAGMENT_HEADER_SIZE;
        size_t newSize = reassemblySize + fragmentDataSize;

        uint8_t* newBuffer = (uint8_t*)realloc(reassemblyBuffer, newSize + 1); // +1 for null terminator
        if (!newBuffer) {
            DEBUG_PRINTLN(F("ESP-NOW fragment reassembly: memory allocation failed"));
            cleanupReassembly();
            return true; // Handled by failing gracefully
        }

        reassemblyBuffer = newBuffer;

        // Copy fragment data
        memcpy(reassemblyBuffer + reassemblySize, payload + ESPNOW_FRAGMENT_HEADER_SIZE, fragmentDataSize);
        reassemblySize = newSize;
        fragmentsReceived++;

        // Check if we have all fragments
        if (fragmentsReceived >= totalFragments) {
            reassemblyBuffer[reassemblySize] = '\0'; // Null terminate
            DEBUG_PRINTF_P(PSTR("ESP-NOW complete message reassembled (%d bytes): %s\n"), reassemblySize, (char*)reassemblyBuffer);

            // Mark this message as processed for deduplication
            lastProcessedMsgId = messageId;

            // Process the complete JSON message
            if (requestJSONBufferLock(18)) {
                DeserializationError error = deserializeJson(*pDoc, reassemblyBuffer, reassemblySize);
                JsonObject root = pDoc->as<JsonObject>();
                if (!error && !root.isNull()) {
                    deserializeState(root);
                }
                else {
                    DEBUG_PRINTF_P(PSTR("ESP-NOW JSON deserialization error: %s\n"), error.c_str());
                }
                releaseJSONBufferLock();
            }

            // Clean up
            cleanupReassembly();
        }

        return true; // Message was handled
    }

    uint16_t getId() override {
        return USERMOD_ID_ESPNOW_JSON;
    }
};

// Allocate static instance and register with WLED
static EspNowJsonHandler espNowJsonHandler;
REGISTER_USERMOD(espNowJsonHandler);

#endif // WLED_DISABLE_ESPNOW
