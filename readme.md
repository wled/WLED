# This is QM's WLED fork for best support of the Deadline'24 Trophy ever
.. and there's oh-so-little one can do about it.

Why fork, you might ask? Because multiple reasons:
 * Hard limit of the maximum current
   * Due to a lot of LEDs tightly packed within epoxy, these can get too hot otherwise and kill the circuit
   * these CAN NOT be fixed!
 * Fixed LED segments for base, logo, and the single-white LEDs at the floor and back side
 * ability to constantly send the current RGB values via UDP
   * in order to work with my Trophy Smiuluator at https://github.com/qm210/dltrophy-simulator
 * easier development using a build image (see below)

Also, if these don't convince you, well – fork you ¯\\\_(ツ)\_/¯

## If you are one of these losers who do not have a Trophy yet
...good news that I had several ounces of boque to invest into the [Simulator](https://github.com/qm210/dltrophy-simulator)

You can use this
* as long as you have a single ESP32 on your own
  * should work for a ESP32-WROOM-32D with at least 4 MB Flash
  * but try whatevery you have and tell me about it
* and as long as you get the Simulator built
  * so far, only tested on a Windows 11, but this will expand to whatever we need

So then you can run this WLED fork on the controller without any LEDs attached and visualize the theoretical output.

# Build & Upload via PlatformIO
If you hate using such build containers (because you feel irrationally smart or something) or have PlatformIO anyway:

Take PlatformIO (i.e. [PlatformIO for VSCode](https://platformio.org/install/ide?install=vscode)), download the repo, connect the controller via COM port, upload the shit via PlatformIO and tell me where it breaks.

### Upload fails?
* Always check that the USB cable supports Data, not only Charging.
* with `serial.serialutil.SerialTimeoutException: Write timeout`
  * Sometimes (not always) you have to explicitly enter Bootloader mode, i.e. keep the BOOT button pressed before the output says `Serial port COM5` / `Connecting.....`
    * if it is then `Writing at ...`, you can stop pressing
  * Is power supply sufficient?
    * Try different USB ports on your machine
  * is the upload speed too high? check `platformio_override.ini`
    * `upload_speed = 115200` should work
  * Check OS driver issues? (I admin I never had that, but people in that internet did so)
  * might be multiple of the above.

### Uploading via help of what WLED already gives us
If you have a new ESP32 without any WLED on it, you should be able to use the official WLED web installer https://install.wled.me/ that flashes the whole ESP32 with the usual, everyone has it, boring Non-Deadline-WLED (see also https://kno.wled.ge/basics/install-binary/)
* and then set it up, and from the Settings -> Sync & Updates -> you can flash your binary Over-The-Air (OTA)
* Remember that OTA-patching the firmware works on two same-size partitions, switching between each other, so if the Update fails, you still have the previous fallback.

### Getting to work
** If there is something not working, either directly tell QM or open an issue in this repo **
* Files to check, maybe the-same-QM committed some... crime:
  * `platformio_override.ini`
  * `my_config.h`
    * especially `#define RESET_CONFIG`, that will remove your device's config files on reboot (good for development, bad for if you actually want to config)
  * `usermods/DEADLINE_TROPHY/config_defines.h`
* Can't connect to the AP? is there one?
  * Can you connect to the Serial Monitor (115200 baud)?
    * Is there any sense here?
      * also: if anything is in there spamming into the monitor in an endless loop, that's a super-bug, it can never do its actual job.
    * Do you see some message along `WiFi: ...`
    * Do you see `No connection configured.` or anything with `Access point...`? What is written below?
  * Seeing any obvious WiFi SSIDs like `DL-TROPHY-AP` or something in your proximity?
    * maybe try some `deadline` or `deadline2024` PW if this is QM's not-overly-hacking-proof-default
      (...that he only uses for the deadline trophy development and his crypto wallets!)
    * connected? try `http://4.3.2.1`
    * Checking the Serial Monitor... is there some `WiFi-E: AP Client Connected (1) @ 407s.`?

_Unless you changed it_ (and in that case you're clearly not the audience for this part), the Deadline WLED sets the following network variables:

| Variable        | Default Value  | Senf  |
|-----------------|----------------|-------|
| `WLED_AP_SSID`  | "DL-TROPHY-AP" | search your surrounding WiFis for that    |
| `WLED_AP_PASS`  | "deadline"     | --    |
| `WLED_OTA_PASS` | "deadlota"     | is sometimes not asked, I wonder why |
| `MDNS_NAME`     | "deadline"     | to use mDNS, your network must support it, and SCOtty told me he turned it off in the ORWOhaus. <br/>But _if_ you have it, you can access the Web UI then under `http://deadline.local` |

#### Stuff to Change
Obviously, no one can stop you from changing whatever you find here, but if you just want to develop a pattern of your own, look for
* `wled00/my_config.h`
  * for some overall define flags (it should contain some documentation, speaking as of now)
* `wled00/FX_DEADLINE_TROPHY.h`
  * contains the function that is called for every time step and segment
  * modify this to make your own pattern instead
  * for inspiration, you can look to the [existing patterns in FX.cpp](wled00/FX.cpp)

# Build via Container
Less requirements on your local drive, and less pollution, and maybe less confusing – just as an option.
 * If you fail, just ask me (the qm dude)
   * If you are half as insufferable as your hate against images suggests, I will be glad to help :)
   * If you fall into the wrong half - I don't give a shit.

Now:
Can be built using Docker or the more lightweight [Podman](https://podman.io/docs/installation), as
```
podman build -t trophy-builder .
```
Then have your modifications in the `data/` folder:
 * `data/FX_DEADLINE_TROPHY.h` - your pattern (there will be some documentation somewhere on what to actually do)
 * `data/my_config.h` - your global config flags via `#define`, e.g. network settings blablahblooblooh

The `my_config.h` should at least contain the credentials for your WiFi in order to access the UI on http://deadline.local (default, configurable later on)
```
#define CLIENT_SSID "..."
#define CLIENT_PASS "..."
```

then mount these into the container (depending on your environment) and run the thing;
```
# Linux
podman run --rm -v $(pwd)/data:/mnt trophy-builder
# Windows PowerShell
podman run --rm -v ${PWD}/data:/mnt trophy-builder
# Windows Command Prompt
podman run --rm -v %cd%\data:/mnt trophy-builder
```
This will then run platformio and produce a `data/firmware.bin` (also one called `deadline_trophy_<Timestamp>.bin`, just in case you tend to get distracted...).

### Write the .bin from pio per CLI
```
# in the project root directory (where the platformio[_override].ini live):
rm -r -fo .pio/build

& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e "deadline_trophy"

cp .pio/build/deadline_trophy/firmware.bin <target>
```

## Upload the .bin to the Controller
One should be able to access the UI.

The resulting `firmware.bin` can be uploaded via WLED's integrated Over-The-Air Updater, via
* Web UI -> Config -> Security & Updates -> Manual OTA Update
  * e.g. [http://<WLED host>/update](http://deadline.local/update) (...unless you changed that hostname.)
* Select the bin file and **Update!**

There are manual ways of doing this with `esptool` etc. but this required advanced knowledge about the partitioning,
i.e. I can prepare that documentation but if no one ever needs it, won't waste my time now... gotta do important... shrooms, or something.

This fork uses `deadline.local` as default DNS name, i.e. you can then access the UI under http://deadline.local, when in your configured network. You can also pass

### Compiler Flags
There are a lot, but these are most relevant to know
```
// local network access
#define CLIENT_SSID "guess what"
#define CLIENT_PASS "won't tell you"

// to access the UI under http://whatever.local
#define MDNS_NAME whatever

// for the Simulator
#define DEADLINE_UDP_SENDER_IP "192.168.178.20"
#define DEADLINE_UDP_SENDER_PORT 3413

// if set, this will always delete the config file on the controller at boot
#define RESET_CONFIG

// if set, this will increase logging to read from any Serial monitor (baud 115200)
#define WLED_DEBUG
```

### Summary: Developer Workflow
So, ideally, for the time being, you do (in this project root folder)
 * Set Up (once or seldom)
   * `podman build -t trophy-builder .`
     * for a virgin controller: also `podman run <...see below...>` the thing once to get the `partitions.bin`, then
       `esptool.py --chip esp32 --port <PORT> write_flash 0x8000 .\data\partition.bin`
       * **DOCS ARE WORK-IN-PROGRESS HERE**
   * adjust `data/my_config.h`
 * Then all day long (example PowerShell, adjust as required)
   * adjust `data/FX_DEADLINE_TROPHY.h` according to whatever floats your goat
   * `podman run --rm -v ${PWD}/data:/mnt trophy-builder`
   * `esptool --port COM5 write-flash -z 0x10000 .\data\firmware.bin`
 * And when the weather starts getting harsher, remember that once again, it might be Deadline soon.

### Making your FX Audio Reactive
 * we use the official AudioReactive mod for audio sync, this fork just has fixed I2S microphone pin settings
 * i.e. to implement a pattern reacting to sound, search for any other FX that already uses something like
 ```
 um_data_t *um_data = getAudioData();
 uint8_t *fftResult = (uint8_t*)um_data->u_data[2];
 ```
 * from `audio_reactive.cpp` we can see the slots of um_data:
 ```
 - setup()
	// we will assign all usermod exportable data here as pointers to original variables or arrays and allocate memory for pointers
	um_data = new um_data_t;
	um_data->u_size = 8;
	um_data->u_type = new um_types_t[um_data->u_size];
	um_data->u_data = new void*[um_data->u_size];
	um_data->u_data[0] = &volumeSmth;      //*used (New)
	um_data->u_type[0] = UMT_FLOAT;
	um_data->u_data[1] = &volumeRaw;      // used (New)
	um_data->u_type[1] = UMT_UINT16;
	um_data->u_data[2] = fftResult;        //*used (Blurz, DJ Light, Noisemove, GEQ_base, 2D Funky Plank, Akemi)
	um_data->u_type[2] = UMT_BYTE_ARR;
	um_data->u_data[3] = &samplePeak;      //*used (Puddlepeak, Ripplepeak, Waterfall)
	um_data->u_type[3] = UMT_BYTE;
	um_data->u_data[4] = &FFT_MajorPeak;   //*used (Ripplepeak, Freqmap, Freqmatrix, Freqpixels, Freqwave, Gravfreq, Rocktaves, Waterfall)
	um_data->u_type[4] = UMT_FLOAT;
	um_data->u_data[5] = &my_magnitude;   // used (New)
	um_data->u_type[5] = UMT_FLOAT;
	um_data->u_data[6] = &maxVol;          // assigned in effect function from UI element!!! (Puddlepeak, Ripplepeak, Waterfall)
	um_data->u_type[6] = UMT_BYTE;
	um_data->u_data[7] = &binNum;          // assigned in effect function from UI element!!! (Puddlepeak, Ripplepeak, Waterfall)
	um_data->u_type[7] = UMT_BYTE;

 - and their definition
    // new "V2" audiosync struct - 44 Bytes
    struct __attribute__ ((packed)) audioSyncPacket {  // "packed" ensures that there are no additional gaps
      char    header[6];      //  06 Bytes  offset 0
      uint8_t reserved1[2];   //  02 Bytes, offset 6  - gap required by the compiler - not used yet
      float   sampleRaw;      //  04 Bytes  offset 8  - either "sampleRaw" or "rawSampleAgc" depending on soundAgc setting
      float   sampleSmth;     //  04 Bytes  offset 12 - either "sampleAvg" or "sampleAgc" depending on soundAgc setting
      uint8_t samplePeak;     //  01 Bytes  offset 16 - 0 no peak; >=1 peak detected. In future, this will also provide peak Magnitude
      uint8_t reserved2;      //  01 Bytes  offset 17 - for future extensions - not used yet
      uint8_t fftResult[16];  //  16 Bytes  offset 18
      uint16_t reserved3;     //  02 Bytes, offset 34 - gap required by the compiler - not used yet
      float  FFT_Magnitude;   //  04 Bytes  offset 36
      float  FFT_MajorPeak;   //  04 Bytes  offset 40
    };
 ```

 * As the official AudioReactive integration supports [UDP Sound Sync](https://mm.kno.wled.ge/WLEDSR/UDP-Sound-Sync/)
   you can - for those without a Trophy or otherwise a microphone - feed your controller corresponding audio sync data via UDP.
 * Maybe TODO:
   * if this becomes relevant, I might write a tool to analyse a track and then play it back after some delay (so it looks better with latency)
     * but until then, use https://github.com/netmindz/WLED-sync or (under Windows) https://github.com/Victoare/SR-WLED-audio-server-win to develop.
 * maybe the Deadline Trophy usermod also gets equipped with a UDP feature to accept arbitrary control data (i.e. for MIDI controlling)
   or also to fine-control the time cursor of your current pattern (so you can develop your audio sync more tightly),
   but this will only be relevant when I find that someone could really use it.

## Troubleshooting

#### the Containerized Build
for troubleshooting in Podman: (if you use Docker, I assume you know your stuff anyway.)
```
# make sure the VM is actually running, otherwise call these two and build again
podman machine init
podman machine start

# might come handy:
podman images

# if there is garbage, you can remove these by
#   podman rmi <id> (--force)
#   podman image prune
# see running containers
#   podman ps --all
```


## Forked from the offical WLED release 0.16.0
 * https://github.com/wled/WLED
 * well, forked earlier, but rebased since then.
 * Licensed under the EUPL v1.2 license
   * Credits [here](https://kno.wled.ge/about/contributors/)!

