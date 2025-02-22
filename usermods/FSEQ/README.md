### Configure PlatformIO

Add the following configuration to your `platformio_override.ini` (or `platformio.ini`) to enable the necessary build flags and define the usermod:

```ini
[env:esp32dev_V4]
build_flags = 
  -D WLED_DEBUG
  -D WLED_USE_SD_SPI
  -D USERMOD_FSEQ
  -std=gnu++17
  -I wled00/src/dependencies/json























# SD & FSEQ Usermod for WLED

This usermod adds support for playing FSEQ files from an SD card and provides a web interface for managing SD files and FSEQ playback. Follow the instructions below to install and use this usermod.

---

## Installation

### 1. Modify `usermods_list.cpp`

Add the following lines to register the FSEQ usermod. Make sure that you **do not** include any conflicting modules (e.g. `usermod_sd_card.h`):

```cpp
#ifdef USERMOD_FSEQ
  #include "../usermods/FSEQ/usermod_fseq.h"
#endif

#ifdef USERMOD_FSEQ
  UsermodManager::add(new UsermodFseq());
#endif

// #include "../usermods/sd_card/usermod_sd_card.h"


### 2. Configure PlatformIO

Add the following configuration to your platformio_override.ini (or platformio.ini) to enable the necessary build flags and define the usermod:


[env:esp32dev_V4]
build_flags = 
  -D WLED_DEBUG
  -D WLED_USE_SD_SPI
  -D WLED_PIN_SCK=18    ; CLK
  -D WLED_PIN_MISO=19   ; Data Out (POCI)
  -D WLED_PIN_MOSI=23   ; Data In (PICO)
  -D WLED_PIN_SS=5      ; Chip Select (CS)
  -D USERMOD_FSEQ
  -std=gnu++17



### 3. Update the WLED Web Interface

To integrate the new FSEQ functionality into the WLED UI, add a new button in your index.htm file. For example, insert the following button into the navigation area:

<!-- New button for SD & FSEQ Manager -->
<button onclick="window.location.href=getURL('/sd/ui');">
  <i class="icons">&#xe0d2;</i>
  <p class="tab-label">Fseq</p>
</button>


This button will take you to the SD & FSEQ Manager interface.



###Usage
	•	Web Interface:
Access the SD & FSEQ Manager by clicking the Fseq button in the main UI. The interface allows you to view, upload, and delete SD card files as well as control FSEQ playback.
	•	File Management:
The SD file manager displays all files on the SD card. You can upload new files via the provided form and delete files using the red “Delete” button. 
	•	FSEQ Playback:
Once an FSEQ file is loaded, the usermod will play the sequence on the LED strip. Use the provided web interface to start and stop playback.





