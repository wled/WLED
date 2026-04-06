/**
 * Structs for passing around usermod state
 */
#pragma once

#include <pixels_dice_interface.h>  // https://github.com/axlan/arduino-pixels-dice

/**
 * Here's how the rolls are tracked in this usermod.
 * 1. The arduino-pixels-dice library reports rolls and state mapped to
 *    PixelsDieID.
 * 2. The "configured_die_names" sets which die to connect to and their order.
 * 3. The rest of the usermod references the die by this order (ie. the LED
 *    effect is triggered for rolls for die 0).
 */

static constexpr size_t MAX_NUM_DICE = 2;
static constexpr uint8_t INVALID_ROLL_VALUE = 0xFF;

/**
 * The state of the connected die, and new events since the last update.
 */
struct DiceUpdate {
  // The vectors to hold results queried from the library
  // Since vectors allocate data, it's more efficient to keep reusing an instance
  // instead of declaring them on the stack.
  std::vector<pixels::PixelsDieID> dice_list;
  pixels::RollUpdates roll_updates;
  pixels::BatteryUpdates battery_updates;
  // The PixelsDieID for each dice index. 0 if the die isn't connected.
  // The ordering here matches configured_die_names.
  std::array<pixels::PixelsDieID, MAX_NUM_DICE> connected_die_ids{0, 0};
};

struct DiceSettings {
  // The mapping of dice names, to the index of die used for effects (ie. The
  // die named "Cat" is die 0). BLE discovery will stop when all the dice are
  // found. The die slot is disabled if the name is empty. If the name is "*",
  // the slot will use the first unassociated die it sees.
  std::array<std::string, MAX_NUM_DICE> configured_die_names{"*", "*"};
  // A label set to describe the next die roll. Index into GetRollName().
  uint8_t roll_label = INVALID_ROLL_VALUE;
};

// These are updated in the main loop, but accessed by the effect functions as
// well. My understand is that both of these accesses should be running on the
// same "thread/task" since WLED doesn't directly create additional threads. The
// exception would be network callbacks and interrupts, but I don't believe
// these accesses are triggered by those. If synchronization was needed, I could
// look at the example in `requestJSONBufferLock()`.
std::array<pixels::RollEvent, MAX_NUM_DICE> last_die_events;

static pixels::RollEvent GetLastRoll() {
  pixels::RollEvent last_roll;
  for (const auto& event : last_die_events) {
    if (event.timestamp > last_roll.timestamp) {
      last_roll = event;
    }
  }
  return last_roll;
}

/**
 * Returns true if the container has an item that matches the value.
 */
template <typename C, typename T>
static bool Contains(const C& container, T value) {
  return std::find(container.begin(), container.end(), value) !=
         container.end();
}

// These aren't known until runtime since they're being added dynamically.
static uint8_t FX_MODE_SIMPLE_D20 = 0xFF;
static uint8_t FX_MODE_PULSE_D20 = 0xFF;
static uint8_t FX_MODE_CHECK_D20 = 0xFF;
std::array<uint8_t, 3> DIE_LED_MODES = {0xFF, 0xFF, 0xFF};
