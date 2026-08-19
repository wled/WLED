# ✨ Usermod FSEQ ✨

> **Created by: Andrej Chrcek**

Welcome to the **Usermod FSEQ** project! This innovative module empowers your WLED setup by enabling FSEQ file playback from an SD card, complete with a sleek web UI and UDP remote control. Dive into a world where creativity meets functionality and transform your lighting experience.

# SWEB UI http://yourIP/sd/ui

# SD & FSEQ Usermod for WLED

This usermod adds support for playing FSEQ files from an SD card and provides a web interface for managing SD files and controlling FSEQ playback via HTTP and UDP. It also supports configurable SPI pin settings for SD SPI. The usermod exposes several HTTP endpoints for file management and playback control.

---

## Features

- **FSEQ Playback:** Play FSEQ files from an SD card.
- **Web UI:** Manage SD files (list, upload, delete) and control FSEQ playback.
- **UDP Synchronization:** Remote control via UDP packets.
- **Configurable SPI Pins:** SPI pin assignments are configurable through WLED’s configuration JSON.

---

## Installation

### 1. Configure PlatformIO

Add the following configuration to your `platformio_override.ini` (or `platformio.ini`) file:

```ini
[env:esp32dev_V4]
build_flags = 
  -D WLED_USE_SD_SPI
  -D USERMOD_FPP
  -D USERMOD_FSEQ
  -I wled00/src/dependencies/json
  ; optional:
  ; -D WLED_DEBUG


### 2. Update the WLED Web Interface (Optional)

To integrate the new FSEQ functionality into the WLED UI, add a new button to the navigation area in your `wled00/data/index.htm` file. For example:

<!-- New button for SD & FSEQ Manager -->
<button onclick="window.location.href=getURL('/fsequi');">
  <i class="icons">&#xe0d2;</i>
  <p class="tab-label">Fseq</p>
</button>

### 3. Modify usermods_list.cpp

Register the FSEQ usermod by adding the following lines to `usermods_list.cpp`. Ensure no conflicting modules are included:

```cpp
#ifdef USERMOD_FSEQ
  #include "../usermods/FSEQ/usermod_fseq.h"
#endif

#ifdef USERMOD_FSEQ
  UsermodManager::add(new UsermodFseq());
#endif

// Remove or comment out any conflicting SD card usermod:
// //#include "../usermods/sd_card/usermod_sd_card.h"
// //#ifdef SD_ADAPTER
// //UsermodManager::add(new UsermodSdCard());
// //#endif

HTTP Endpoints

The following endpoints are available:
	• GET /sd/ui  
	  Description: Returns an HTML page with the SD & FSEQ Manager interface.  
	  Usage: Open this URL in a browser.
	• GET /sd/list  
	  Description: Displays an HTML page listing all files on the SD card, with options to delete files and a form to upload new files.  
	  Usage: Access this URL to view the SD file list.
	• POST /sd/upload  
	  Description: Handles file uploads to the SD card using a multipart/form-data POST request.  
	  Usage: Use a file upload form or an HTTP client.
	• GET /sd/delete  
	  Description: Deletes a specified file from the SD card. Requires a query parameter path indicating the file path.  
	  Usage: Example: /sd/delete?path=/example.fseq.
	• GET /fseq/list  
	  Description: Returns an HTML page listing all FSEQ files (with .fseq or .FSEQ extensions) on the SD card. Each file includes a button to play it.  
	  Usage: Open this URL in a browser to view and interact with the file list.
	• GET /fseq/start  
	  Description: Starts playback of a selected FSEQ file. Requires a file query parameter and optionally a t parameter (time offset in seconds).  
	  Usage: Example: /fseq/start?file=/animation.fseq&t=10.
	• GET /fseq/stop  
	  Description: Stops the current FSEQ playback and clears the active playback session.  
	  Usage: Send an HTTP GET request to stop playback.
	• GET /fseqfilelist  
	  Description: Returns a JSON list of all FSEQ files found on the SD card.  
	  Usage: Open this URL or send an HTTP GET request to retrieve the file list.


Configurable SPI Pin Settings

The default SPI pin assignments for SD SPI are defined as follows:

#ifdef WLED_USE_SD_SPI
int8_t UsermodFseq::configPinSourceSelect = 5;
int8_t UsermodFseq::configPinSourceClock  = 18;
int8_t UsermodFseq::configPinPoci         = 19;
int8_t UsermodFseq::configPinPico         = 23;
#endif

These values can be modified via WLED’s configuration JSON using the addToConfig() and readFromConfig() methods. This allows you to change the pin settings without recompiling the firmware.

Summary

The SD & FSEQ Usermod for WLED enables FSEQ file playback from an SD card with a full-featured web UI and UDP synchronization for remote control. With configurable SPI pin settings, this usermod integrates seamlessly into WLED, providing additional functionality without modifying core code.

For further customization or support, please refer to the project documentation or open an issue on GitHub.

