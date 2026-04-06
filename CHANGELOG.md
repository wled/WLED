## WLED changelog

#### Build 2410270
-   WLED 0.15.0-b7 release
-   Re-license the WLED project from MIT to EUPL (#4194 by @Aircoookie)
-   Fix alexa devices invisible/uncontrollable (#4214 by @Svennte)
-   Add visual expand button on hover (#4172)
-   Usermod: Audioreactive tuning and performance enhancements (by @softhack007)
-   `/json/live` (JSON live data/peek) only enabled when WebSockets are disabled
-   Various bugfixes and optimisations: #4179, #4215, #4219, #4222, #4223, #4224, #4228, #4230

#### Build 2410140
-   WLED 0.15.0-b6 release
-   Added BRT timezone (#4188 by @LuisFadini)
-   Fixed the positioning of the "Download the latest binary" button (#4184 by @maxi4329)
-   Add WLED_AUTOSEGMENTS compile flag (#4183 by @PaoloTK)
-   New 512kB FS parition map for 4MB devices
-   Internal API change: Static PinManager & UsermodManager
-   Change in Improv chip ID and version generation
-   Various optimisations, bugfixes and enhancements (#4005, #4174 & #4175 by @Xevel, #4180, #4168, #4154, #4189 by @dosipod)

#### Build 2409170
-   UI: Introduce common.js in settings pages (size optimisation)
-   Add the ability to toggle the reception of palette synchronizations (#4137 by @felddy)
-   Usermod/FX: Temperature usermod added Temperature effect (example usermod effect by @blazoncek)
-   Fix AsyncWebServer version pin

#### Build 2409140
-   Configure different kinds of busses at compile (#4107 by @PaoloTK)
    - BREAKING: removes LEDPIN and DEFAULT_LED_TYPE compile overrides
-   Fetch LED types from Bus classes (dynamic UI) (#4129 by @netmindz, @blazoncek, @dedehai)
-   Temperature usermod: update OneWire to 2.3.8 (#4131 by @iammattcoleman)

#### Build 2409100
-   WLED 0.15.0-b5 release
-   Audioreactive usermod included by default in all compatible builds (including ESP8266)
-   Demystified some byte definitions of WiZmote ESP-NOW message (#4114 by @ChuckMash)
-   Update usermod "Battery" improved MQTT support (#4110 by @itCarl)
-   Added a usermod for interacting with BLE Pixels Dice (#4093 by @axlan)
-   Allow lower values for touch threshold (#4081 by @RobinMeis)
-   Added POV image effect usermod (#3539 by @Liliputech)
-   Remove repeating code to fetch audio data (#4103 by @netmindz)
-   Loxone JSON parser doesn't handle lx=0 correctly (#4104 by @FreakyJ, fixes #3809)
-   Rename wled00.ino to wled_main.cpp (#4090 by @willmmiles)
-   SM16825 chip support including WW & CW channel swap (#4092)
-   Add stress testing scripts (#4088 by @willmmiles)
-   Improve jsonBufferLock management (#4089 by @willmmiles)
-   Fix incorrect PWM bit depth on Esp32 with XTAL clock (#4082 by @PaoloTK)
-   Devcontainer args (#4073 by @axlan)
-   Effect: Fire2012 optional blur amount (#4078 by @apanteleev)
-   Effect: GEQ fix bands (#4077 by @adrianschroeter)
-   Boot delay option (#4060 by @DedeHai)
-   ESP8266 Audioreactive sync (#3962 by @gaaat98, @netmindz, @softhack007)
-   ESP8266 PWM crash fix (#4035 by @willmmiles)
-   Usermod: Battery fix (#4051 by @Nickbert7)
-   Usermod: Mpu6050 usermod crash fix (#4048 by @willmmiles)
-   Usermod: Internal Temperature V2 (#4033 by @adamsthws)
-   Various fixes and improvements (including build environments to emulate 0.14.0 for ESP8266)

#### Build 2407070
-   Various fixes and improvements (mainly LED settings fix)

#### Build 2406290
-   WLED 0.15.0-b4 release
-   LED settings bus management update (WARNING: only allows available outputs)
-   Add ETH support for LILYGO-POE-Pro (#4030 by @rorosaurus)
-   Update usermod_sn_photoresistor (#4017 by @xkvmoto)
-   Several internal fixes and optimisations
    - move LED_BUILTIN handling to BusManager class
    - reduce max panels (web server limitation)
    - edit WiFi TX power (ESP32)
    - keep current ledmap ID in UI
    - limit outputs in UI based on length
    - wifi.ap addition to JSON Info (JSON API)
    - relay pin init bugfix
    - file editor button in UI
    - ESP8266: update was restarting device on some occasions
    - a bit of throttling in UI (for ESP8266)

#### Build 2406120
-   Update NeoPixelBus to v2.8.0
-   Increased LED outputs one ESP32 using parallel I2S (up to 17)
    - use single/mono I2S + 4x RMT for 5 outputs or less
    - use parallel x8 I2S + 8x RMT for >5 outputs (limit of 300 LEDs per output)
-   Fixed code of Smartnest and updated documentation (#4001 by @DevilPro1)
-   ESP32-S3 WiFi fix (#4010 by @cstruck)
-   TetrisAI usermod fix (#3897 by @muebau)
-   ESP-NOW usermod hook
-   Update wled.h regarding OTA Password (#3993 by @gsieben)
-   Usermod BME68X Sensor Implementation (#3994 by @gsieben)
-   Add a usermod for AHT10, AHT15 and AHT20 temperature/humidity sensors (#3977 by @LordMike)
-   Update Battery usermod documentation (#3968 by @adamsthws)
-   Add INA226 usermod for reading current and power over i2c (#3986 by @LordMike)
-   Bugfixes: #3991
-   Several internal fixes and optimisations (WARNING: some effects may be broken that rely on overflow/narrow width)
    - replace uint8_t and uint16_t with unsigned
    - replace in8_t and int16_t with int
    - reduces code by 1kB

#### Build 2405180
-   WLED 0.14.4 release
-   Fix for #3978
-   Official 0.15.0-b3 release
-   Merge 0.14.3 fixes into 0_15
-   Added Pinwheel Expand 1D->2D effect mapping mode (#3961 by @Brandon502)
-   Add changeable i2c address to BME280 usermod (#3966 by @LordMike)
-   Effect: Firenoise - add palette selection
-   Experimental parallel I2S support for ESP32 (compile time option)
    - increased outputs to 17
    - increased max possible color order overrides
    - use WLED_USE_PARALLEL_I2S during compile
    WARNING: Do not set up more than 256 LEDs per output when using parallel I2S with NeoPixelBus less than 2.9.0
-   Update Usermod: Battery (#3964 by @adamsthws)
-   Update Usermod: BME280 (#3965 by @LordMike)
-   TM1914 chip support (#3913)
-   Ignore brightness in Peek
-   Antialiased line & circle drawing functions
-   Enabled some audioreactive effects for single pixel strips/segments (#3942 by @gaaat98)
-   Usermod Battery: Added Support for different battery types, Optimized file structure (#3003 by @itCarl)
-   Skip playlist entry API (#3946 by @freakintoddles2)
-   various optimisations and bugfixes (#3987, #3978)

#### Build 2405030
-   Using brightness in analog clock overlay (#3944 by @paspiz85)
-   Add Webpage shortcuts (#3945 by @w00000dy)
-   ArtNet Poll reply (#3892 by @askask)
-   Improved brightness change via long button presses (#3933 by @gaaat98)
-   Relay open drain output (#3920 by @Suxsem)
-   NEW JSON API: release info (update page, `info.release`)
-   update esp32 platform to arduino-esp32 v2.0.9 (#3902)
-   various optimisations and bugfixes (#3952, #3922, #3878, #3926, #3919, #3904 @DedeHai)

#### Build 2404120
-   v0.15.0-b3
-   fix for #3896 & WS2815 current saving
-   conditional compile for AA setPixelColor()

#### Build 2404100
-   Internals: #3859, #3862, #3873, #3875
-   Prefer I2S1 over RMT on ESP32
-   usermod for Adafruit MAX17048 (#3667 by @ccruz09)
-   Runtime detection of ESP32 PICO, general PSRAM support
-   Extend JSON API "info" object
    - add "clock" - CPU clock in MHz
    - add "flash" - flash size in MB
-   Fix for #3879
-   Analog PWM fix for ESP8266 (#3887 by @gaaat98)
-   Fix for #3870 (#3880 by @DedeHai)
-   ESP32 S3/S2 touch fix (#3798 by @DedeHai)
-   PIO env. PSRAM fix for S3 & S3 with 4M flash
    - audioreactive always included for S3 & S2
-   Fix for #3889
-   BREAKING: Effect: modified KITT (Scanner) (#3763)

#### Build 2404040
-   WLED 0.14.3 release
-   Fix for transition 0 (#3854, #3832, #3720)
-   Fix for #3855 via #3873 (by @willmmiles)

#### Build 2403280
-   Individual color channel control for JSON API (fixes #3860)
    - "col":[int|string|object|array, int|string|object|array, int|string|object|array]
        int = Kelvin temperature or 0 for black
        string = hex representation of [WW]RRGGBB
        object = individual channel control {"r":0,"g":127,"b":255,"w":255}, each being optional (valid to send {})
        array = direct channel values [r,g,b,w] (w element being optional)
-   runtime selection for CCT IC (Athom 15W bulb)
-   #3850 (by @w00000dy)
-   Rotary encoder palette count bugfix
-   bugfixes and optimisations

#### Build 2403240
-   v0.15.0-b2
-   WS2805 support (RGB + WW + CW, 600kbps)
-   Unified PSRAM use
-   NeoPixelBus v2.7.9 (for future WS2805 support)
-   Ubiquitous PSRAM mode for all variants of ESP32
-   SSD1309_64 I2C Support for FLD Usermod (#3836 by @THATDONFC)
-   Palette cycling fix (add support for `{"seg":[{"pal":"X~Y~"}]}` or `{"seg":[{"pal":"X~Yr"}]}`)
-   FW1906 Support (#3810 by @deece and @Robert-github-com)
-   ESPAsyncWebServer 2.2.0 (#3828 by @willmmiles)
-   Bugfixes: #3843, #3844

#### Build 2403190
-   limit max PWM frequency (fix incorrect PWM resolution)
-   Segment UI bugfix
-   Updated AsyncWebServer (by @wlillmmiles)
-   Simpler boot preset (fix for #3806)
-   Effect: Fix for 2D Drift animation (#3816 by @BaptisteHudyma)
-   Effect: Add twin option to 2D Drift
-   MQTT cleanup
-   DDP: Support sources that don't push (#3833 by @willmmiles)
-   Usermod: Tetris AI usermod (#3711 by @muebau)

#### Build 2403171
-   merge 0.14.2 changes into 0.15

#### Build 2403070
-   Add additional segment options when controlling over e1.31 (#3616 by @demophoon)
-   LockedJsonResponse: Release early if possible (#3760 by @willmmiles)
-   Update setup-node and cache usermods in wled-ci.yml (#3737 by @WoodyLetsCode)
-   Fix preset sorting (#3790 by @WoodyLetsCode)
-   compile time button configuration #3792
-   remove IR config if not compiled
-   additional string optimisations
-   Better low brightness level PWM handling (fixes #2767, #2868)

#### Build 2402290
-   Multiple analog button fix for #3549
-   Preset caching on chips with PSRAM (credit @akaricchi)
-   Fixing stairway usermod and adding buildflags (by @lost-hope)
-   ESP-NOW packet modification
-   JSON buffer lock error messages / Reduce wait time for lock to 100ms
-   Reduce string RAM usage for ESP8266
-   Fixing a potential array bounds violation in ESPDMX
-   Move timezone table to PROGMEM (#3766 by @willmmiles)
-   Reposition upload warning message. (fixes #3778)
-   ABL display fix & optimisation
-   Add virtual Art-Net RGBW option (#3783 by @shammy642)

#### Build 2402090
-   Added new Ethernet controller RGB2Go Tetra (duplicate of ESP3DEUXQuattro)
-   Usermod: httpPullLightControl (#3560 by @roelbroersma)
-   DMX: S2 & C3 support via modified ESPDMX
-   Bugfix: prevent cleaning of JSON buffer after a failed lock attempt (BufferGuard)
-   Product/Brand override (API & AP SSID) (#3750 by @moustachauve)

#### Build 2402060
-   WLED version 0.15.0-b1
-   Harmonic Random Cycle palette (#3729 by @dedehai)
-   Multi PIR sensor usermod (added support for attaching multiple PIR sensors)
-   Removed obsolete (and nonfunctional) usermods

#### Build 2309120 till build 2402010
-   WLED version 0.15.0-a0
-   Multi-WiFi support. Add up to 3 (or more via cusom compile) WiFis to connect to (with help from @JPZV)
-   Temporary AP. Use your WLED in public with temporary AP.
-   Github CI build system enhancements (#3718 by @WoodyLetsCode)
-   Accessibility: Node list ( #3715 by @WoodyLetsCode)
-   Analog clock overlay enhancement (#3489 by @WoodyLetsCode)
-   ESP32-POE-WROVER from Olimex ethernet support (#3625 by @m-wachter)
-   APA106 support (#3580 by @itstefanjanos)
-   BREAKING: Effect: updated Palette effect to support 2D (#3683 by @TripleWhy)
-   "SuperSync" from WLED MM (by @MoonModules)
-   Effect: DNA Spiral Effect Speed Fix (#3723 by @Derek4aty1)
-   Fix for #3693
-   Orange flash fix (#3196) for transitions
-   Add own background image upload (#3596 by @WoodyLetsCode)
-   WLED time overrides (`WLED_NTP_ENABLED`, `WLED_TIMEZONE`, `WLED_UTC_OFFSET`, `WLED_LAT` and `WLED_LON`)
-   Better sorting and naming of static palettes (by @WoodyLetsCode)
-   ANIMartRIX usermod and effects (#3673 by @netmindz)
-   Use canvas instead of CSS gradient for liveview (#3621 by @zanhecht)
-   Fix for #3672
-   ColoOrderMap W channel swap (color order overrides now have W swap)
-   En-/disable LED maps when receiving realtime data (#3554 by @ezcGman)
-   Added PWM frequency selection to UI (Settings)
-   Automatically build UI before compiling (#3598, #3666 by @WoodyLetsCode)
-   Internal: Added *suspend* API to `strip` (`WS2812FX class`)
-   Possible fix for #3589 & partial fix for #3605
-   MPU6050 upgrade (#3654 by @willmmiles)
-   UI internals (#3656 by @WoodyLetsCode)
-   ColorPicker fix (#3658 by @WoodyLetsCode)
-   Global JSON buffer guarding (#3648 by @willmmiles, resolves #3641, #3312, #3367, #3637, #3646, #3447)
-   Effect: Fireworks 1D (fix for matrix trailing strip)
-   BREAKING: Reduced number of segments (12) on ESP8266 due to less available RAM
-   Increased available effect data buffer (increases more if board has PSRAM)
-   Custom palette editor mobile UI enhancement (by @imeszaros)
-   Per port Auto Brightness Limiter (ABL)
-   Use PSRAM for JSON buffer (double size, larger ledmaps, up to 2k)
-   Reduced heap fragmentation by allocating ledmap array only once and not deallocating effect buffer
-   HTTP retries on failed UI load
-   UI Search: scroll to top (#3587 by @WoodyLetsCode)
-   Return to inline iro.js and rangetouch.js (#3597 by @WoodyLetsCode)
-   Better caching (#3591 by @WoodyLetsCode)
-   Do not send 404 for missing `skin.css` (#3590 by @WoodyLetsCode)
-   Simplified UI rework (#3511 by @WoodyLetsCode)
-   Domoticz device ID for PIR and Temperature usermods
-   Bugfix for UCS8904 `hasWhite()`
-   Better search in UI (#3540 by @WoodyLetsCode)
-   Seeding FastLED PRNG (#3552 by @TripleWhy)
-   WIZ Smart Button support (#3547 by @micw)
-   New button type (button switch, fix for #3537)
-   Pixel Magic Tool update (#3483 by @ajotanc)
-   Effect: 2D Matrix fix for gaps
-   Bugfix #3526, #3533, #3561
-   Spookier Halloween Eyes (#3501)
-   Compile time options for Multi Relay usermod (#3498)
-   Effect: Fix for Dissolve (#3502)
-   Better reverse proxy support (nested paths)
-   Implement global JSON API boolean toggle (i.e. instead of "var":true or "var":false -> "var":"t").
-   Sort presets by ID
-   Fix for #3641, #3312, #3367, #3637, #3646, #3447, #3632, #3496, #2922, #3593, #3514, #3522, #3578 (partial), #3606 (@WoodyLetsCode)
-   Improved random bg image and added random bg image options (@WoodyLetsCode, #3481)
-   Audio palettes (Audioreactive usermod, credit @netmindz)
-   Better UI tooltips (@ajotnac, #3464)
-   Better effect filters (filter dropdown)
-   UDP sync fix (for #3487)
-   Power button override (solves #3431)
-   Additional HTTP request throttling (ESP8266)
-   Additional UI/UX improvements
-   Segment class optimisations (internal)
-   ESP-NOW sync
-   ESP-NOW Wiz remote JSON overrides (similar to IR JSON) & bugfixes
-   Gamma correction for custom palettes (#3399).
-   Restore presets from browser local storage
-   Optional effect blending
-   Restructured UDP Sync (internal)
    -   Remove sync receive
    -   Sync clarification
-   Disallow 2D effects on non-2D segments
-   Return of 2 audio simulations
-   Bugfix in sync #3344 (internal)
    -   remove excessive segments
    -   ignore inactive segments if not syncing bounds
    -   send UDP/WS on segment change
    -   pop_back() when removing last segment

#### Build 2403170
-   WLED 0.14.2 release

#### Build 2403110
-   Beta WLED 0.14.2-b2
-   New AsyncWebServer (improved performance and reduced memory use)
-   New builds for ESP8266 with 160MHz CPU clock
-   Fixing stairway usermod and adding buildflags (#3758 by @lost-hope)
-   Fixing a potential array bounds violation in ESPDMX
-   Reduced RAM usage (moved strings and TZ data (by @willmmiles) to PROGMEM)
-   LockedJsonResponse: Release early if possible (by @willmmiles)

#### Build 2402120
-   Beta WLED 0.14.2-b1
-   Possible fix for #3589 & partial fix for #3605
-   Prevent JSON buffer clear after failed lock attempt
-   Multiple analog button fix for #3549
-   UM Audioreactive: add two compiler options (#3732 by @wled-install)
-   Fix for #3693

#### Build 2401141
-   Official release of WLED 0.14.1
-   Fix for #3566, #3665, #3672
-   Sorting of palettes in custom palette editor (#3674 by @WoodyLetsCode)

#### Build 2401060
-   Version bump: 0.14.1-b3
-   Global JSON buffer guarding (#3648 by @willmmiles, resolves #3641, #3312, #3367, #3637, #3646, #3447)
-   Fix for #3632
-   Custom palette editor mobile UI enhancement (#3617 by @imeszaros)
-   changelog update

#### Build 2312290
-   Fix for #3622, #3613, #3609
-   Various tweaks and fixes
-   changelog update

#### Build 2312230
-   Version bump: 0.14.1-b2
-   Fix for Pixel Magic button
-   Fix for #2922 (option to force WiFi PHY mode to G on ESP8266)
-   Fix for #3601, #3400 (incorrect sunrise/sunset, #3612 by @softhack007)

#### Build 2312180
-   Bugfixes (#3593, #3490, #3573, #3517, #3561, #3555, #3541, #3536, #3515, #3522, #3533, #3508)
-   Various other internal cleanups and optimisations

#### Build 2311160
-   Version bump: 0.14.1-b1
-   Bugfixes (#3526, #3502, #3496, #3484, #3487, #3445, #3466, #3296, #3382, #3312)
-   New feature: Sort presets by ID
-   New usermod: LDR sensor (#3490 by @JeffWDH)
-   Effect: Twinklefox & Tinklecat metadata fix
-   Effect: separate #HH and #MM for Scrolling Text (#3480)
-   SSDR usermod enhancements (#3368)
-   PWM fan usermod enhancements (#3414)

#### Build 2310010, build 2310130
-   Release of WLED version 0.14.0 "Hoshi"
-   Bugfixes for #3400, #3403, #3405
-   minor HTML optimizations
-   audioreactive: bugfix for UDP sound sync (partly initialized packets)

#### Build 2309240
-   Release of WLED beta version 0.14.0-b6 "Hoshi"
-   Effect bugfixes and improvements (Meteor, Meteor Smooth, Scrolling Text)
-   audioreactive: bugfixes for ES8388 and ES7243 init; minor improvements for analog inputs

#### Build 2309100
-   Release of WLED beta version 0.14.0-b5 "Hoshi"
-   New standard esp32 build with audioreactive
-   Effect blending bugfixes, and minor optimizations

#### Build 2309050
-   Effect blending (#3311) (finally effect transitions!)
    *WARNING*: May not work well with ESP8266, with plenty of segments or usermods (low RAM condition)!!!
-   Added receive and send sync groups to JSON API (#3317) (you can change sync groups using preset)
-   Internal temperature usermod (#3246)
-   MQTT server and topic length overrides (#3354) (new build flags)
-   Animated Staircase usermod enhancement (#3348) (on/off toggle/relay control)
-   Added local time info to Info page (#3351)
-   New effect: Rolling Balls (a.k.a. linear bounce) (#1039)
-   Various bug fixes and enhancements.

#### Build 2308110
-   Release of WLED beta version 0.14.0-b4 "Hoshi"
-   Reset effect data immediately upon mode change

#### Build 2308030
-   Improved random palette handling and blending
-   Soap bugfix
-   Fix ESP-NOW crash with AP mode Always

#### Build 2307180
-   Bus-level global buffering (#3280)
-   Removed per-segment LED buffer (SEGMENT.leds)
-   various fixes and improvements (ESP variants platform 5.3.0, effect optimizations, /json/cfg pin allocation)

#### Build 2307130
-   larger `oappend()` stack buffer (3.5k) for ESP32
-   Preset cycle bugfix (#3262)
-   Rotary encoder ALT fix for large LED count (#3276)
-   effect updates (2D Plasmaball), `blur()` speedup
-   On/Off toggle from nodes view (may show unknown device type on older versions) (#3291)
-   various fixes and improvements (ABL, crashes when changing presets with different segments)

#### Build 2306270
-   ESP-NOW remote support (#3237)
-   Pixel Magic tool (display pixel art) (#3249)
-   Websocket (peek) fallback when connection cannot be established, WS retries (#3267)
-   Add WiFi network scan RPC command to Improv Serial (#3271)
-   Longer (custom option available) segment name for ESP32
-   various fixes and improvements

#### Build 2306210
-   0.14.0-b3 release
-   respect global I2C in all usermods (no local initialization of I2C bus)
-   Multi relay usermod compile-time enabled option (-D MULTI_RELAY_ENABLED=true|false)

#### Build 2306180
-   Added client-side option for applying effect defaults from metadata
-   Improved ESP8266 stability by reducing WebSocket response resends
-   Updated ESP8266 core to 3.1.2

#### Build 2306141
-   Lissajous improvements
-   Scrolling Text improvements (leading 0)

#### Build 2306140
-   Add settings PIN (un)locking to JSON post API

#### Build 2306130
-   Bumped version to 0.14-b3 (beta 3)
-   added pin dropdowns in LED preferences (not for LED pins) and usermods
-   introduced (unused ATM) NeoGammaWLEDMethod class
-   Reverse proxy support
-   PCF8754 support for Rotary encoder (requires wiring INT pin to ESP GPIO)
-   Rely on global I2C pins for usermods (breaking change)
-   various fixes and enhancements

#### Build 2306020
-   Support for segment sets (PR #3171)
-   Reduce sound simulation modes to 2 to facilitate segment sets
-   Trigger button immediately on press if all configured presets are the same (PR #3226)
-   Changes for allowing Alexa to change light color to White when auto-calculating from RGB (PR #3211)

#### Build 2305280
-   DDP protocol update (#3193)
-   added PCF8574 I2C port expander support for Multi relay usermod
-   MQTT multipacket (fragmented) message fix
-   added option to retain MQTT brightness and color messages
-   new ethernet board: @srg74 Ethernet Shield
-   new 2D effects: Soap (#3184) & Octopus & Waving cell (credit @St3P40 https://github.com/80Stepko08)
-   various fixes and enhancements

#### Build 2305090
-   new ethernet board: @Wladi ABC! WLED Eth
-   Battery usermod voltage calculation (#3116)
-   custom palette editor (#3164)
-   improvements in Dancing Shadows and Tartan effects
-   UCS389x support
-   switched to NeoPixelBus 2.7.5 (replaced NeoPixelBrightnessBus with NeoPixelBusLg)
-   SPI bus clock selection (for LEDs) (#3173)
-   DMX mode preset fix (#3134)
-   iOS fix for scroll (#3182)
-   Wordclock "Norddeutsch" fix (#3161)
-   various fixes and enhancements

#### Build 2304090
-   updated Arduino ESP8266 core to 4.1.0 (newer compiler)
-   updated NeoPixelBus to 2.7.3 (with support for UCS890x chipset)
-   better support for ESP32-C3, ESP32-S2 and ESP32-S3 (Arduino ESP32 core 5.2.0)
-   iPad/tablet with 1024 pixels width in landscape orientation PC mode support (#3153)
-   fix for Pixel Art Converter (#3155)

#### Build 2303240
-   Peek scaling of large 2D matrices
-   Added 0D (1 pixel) metadata for effects & enhance 0D (analog strip) UI handling
-   Added ability to disable ADAlight (-D WLED_DISABLE_ADALIGHT)
-   Fixed APA102 output on Ethernet enabled controllers
-   Added ArtNet virtual/network output (#3121)
-   Klipper usermod (#3106)
-   Remove DST from CST timezone
-   various fixes and enhancements

#### Build 2302180

-   Removed Blynk support (servers shut down on 31st Dec 2022)
-   Added `ledgap.json` to complement ledmaps for 2D matrices
-   Added support for white addressable strips (#3073)
-   Ability to use SHT temperature usermod with PWM fan usermod
-   Added `onStateChange()` callback to usermods (#3081)
-   Refactored `bus_manager` [internal]
-   Dual 1D & 2D mode (add 1D strip after the matrix)
-   Removed 1D -> 2D mapping for individual pixel control
-   effect tweak: Fireworks 1D
-   various bugfixes

#### Build 2301240

-   Version bump to v0.14.0-b2 "Hoshi"
-   PixelArt converter (convert any image to pixel art and display it on a matrix) (PR #3042)
-   various effect updates and optimisations
    -   added Overlay option to some effects (allows overlapping segments)
    -   added gradient text on Scrolling Text
    -   added #DDMM, #MMDD & #HHMM date and time options for Scrolling Text effect (PR #2990)
    -   deprecated: Dynamic Smooth, Dissolve Rnd, Solid Glitter
    -   optimised & enhanced loading of default values
    -   new effect: Distortion Waves (2D)
    -   2D support for Ripple effect
    -   slower minimum speed for Railway effect
-   DMX effect mode & segment controls (PR #2891)
-   Optimisations for conditional compiles (further reduction of code size)
-   better UX with effect sliders (PR #3012)
-   enhanced support for ESP32 variants: C3, S2 & S3
-   usermod enhancements (PIR, Temperature, Battery (PR #2975), Analog Clock (PR #2993))
-   new usermod SHT (PR #2963)
-   2D matrix set up with gaps or irregular panels (breaking change!) (PR #2892)
-   palette blending/transitions
-   random palette smooth changes
-   hex color notations in custom palettes
-   allow more virtual buses
-   plethora of bugfixes

### WLED release 0.14.0-b1

#### Build 2212222

-   Version bump to v0.14.0-b1 "Hoshi"
-   2D matrix support (including mapping 1D effects to 2D and 2D peek)
-   [internal] completely rewritten Segment & WS2812FX handling code
-   [internal] ability to add custom effects via usermods
-   [internal] set of 2D drawing functions
-   transitions on every segment (including ESP8266)
-   enhanced old and new 2D effects (metadata: default values)
-   custom palettes (up to 10; upload palette0.json, palette1.json, ...)
-   custom effect sliders and options, quick filters
-   global I2C and SPI GPIO allocation (for usermods)
-   usermod settings page enhancements (dropdown & info)
-   asynchronous preset loading (and added "pd" JSON API call for direct preset apply)
-   new usermod Boblight (PR #2917)
-   new usermod PWM Outputs (PR #2912)
-   new usermod Audioreactive
-   new usermod Word Clock Matrix (PR #2743)
-   new usermod Ping Pong Clock (PR #2746)
-   new usermod ADS1115 (PR #2752)
-   new usermod Analog Clock (PR #2736)
-   various usermod enhancements and updates
-   allow disabling pull-up resistors on buttons
-   SD card support (PR #2877)
-   enhanced HTTP API to support custom effect sliders & options (X1, X2, X3, M1, M2, M3)
-   multiple UDP sync message retries (PR #2830)
-   network debug printer (PR #2870)
-   automatic UI PC mode on large displays
-   removed support for upgrading from pre-0.10 (EEPROM)
-   support for setting GPIO level when LEDs are off (RMT idle level, ESP32 only) (PR #2478)
-   Pakistan time-zone (PKT)
-   ArtPoll support
-   TM1829 LED support
-   experimental support for ESP32 S2, S3 and C3
-   general improvements and bugfixes

### WLED release 0.13.3

-   Version bump to v0.13.3 "Toki"
-   Disable ESP watchdog by default (fixes flickering and boot issues on a fresh install)
-   Added support for LPD6803

### WLED release 0.13.2

#### Build 2208140

-   Version bump to v0.13.2 "Toki"
-   Added option to receive live data on the main segment only (PR #2601)
-   Enable ESP watchdog by default (PR #2657)
-   Fixed race condition when saving bus config
-   Better potentiometer filtering (PR #2693)
-   More suitable DMX libraries (PR #2652)
-   Fixed outgoing serial TPM2 message length (PR #2628)
-   Fixed next universe overflow and Art-Net DMX start address (PR #2607)
-   Fixed relative segment brightness (PR #2665)

### Builds between releases 0.13.1 and 0.13.2

#### Build 2203191

-   Fixed sunrise/set calculation (once again)

#### Build 2203190

-   Fixed `/json/cfg` unable to set busses (#2589)
-   Fixed Peek with odd LED counts > 255 (#2586)

#### Build 2203160

-   Version bump to v0.13.2-a0 "Toki"
-   Add ability to skip up to 255 LEDs
-   Dependency version bumps

### WLED release 0.13.1

#### Build 2203150

-   Version bump to v0.13.1 "Toki"
-   Fix persistent preset bug, preventing save of new presets

### WLED release 0.13.0

#### Build 2203142

-   Release of WLED v0.13.0 "Toki"
-   Reduce APA102 hardware SPI frequency to 5Mhz
-   Remove `persistent` parameter in `savePreset()`

### Builds between releases 0.12.0 and 0.13.0

#### Build 2203140

-   Added factory reset by pressing button 0 for >10 seconds
-   Added ability to set presets from DMX Effect mode
-   Simplified label hiding JS in user interface
-   Fixed JSON `{"live":true}` indefinite realtime mode

#### Build 2203080

-   Disabled auto white mode in segments with no RGB bus
-   Fixed hostname string not 0-terminated 
-   Fixed Popcorn mode not lighting first LED on pop

#### Build 2203060

-   Dynamic hiding of unused color controls in UI (PR #2567)
-   Removed native Cronixie support and added Cronixie usermod
-   Fixed disabled timed preset expanding calendar
-   Fixed Color Order setting shown for analog busses
-   Fixed incorrect operator (#2566)

#### Build 2203011

-   IR rewrite (PR #2561), supports CCT
-   Added locate button to Time settings
-   CSS fixes and adjustments
-   Consistent Tab indentation in index JS and CSS
-   Added initial contribution style guideline

#### Build 2202222

-   Version bump to 0.13.0-b7 "Toki"
-   Fixed HTTP API commands not applying to all selected segments in some conditions
-   Blynk support is not compiled in by default on ESP32 builds

#### Build 2202210

-   Fixed HTTP API commands not applying to all selected segments if called from JSON
-   Improved Stream effects, no longer rely on LED state and won't fade out at low brightness

#### Build 2202200

-   Added `info.leds.seglc` per-segment light capability info (PR #2552)
-   Fixed `info.leds.rgbw` behavior
-   Segment bounds sync (PR #2547)
-   WebSockets auto reconnection and error handling
-   Disable relay pin by default (PR #2531)
-   Various fixes (ESP32 touch pin 33, floats, PR #2530, #2534, #2538)
-   Deprecated `info.leds.cct`, `info.leds.wv` and `info.leds.rgbw`
-   Deprecated `/url` endpoint

#### Build 2202030

-   Switched to binary format for WebSockets peek (PR #2516)
-   Playlist bugfix
-   Added `extractModeName()` utility function
-   Added serial out (PR #2517)
-   Added configurable baud rate

#### Build 2201260

-   Initial ESP32-C3 and ESP32-S2 support (PRs #2452, #2454, #2502)
-   Full segment sync (PR #2427)
-   Allow overriding of color order by ranges (PR #2463) 
-   Added white channel to Peek

#### Build 2112080

-   Version bump to 0.13.0-b6 "Toki"
-   Added "ESP02" (ESP8266 with 2M of flash) to PIO/release binaries

#### Build 2112070

-   Added new effect "Fairy", replacing "Police All"
-   Added new effect "Fairytwinkle", replacing "Two Areas"
-   Static single JSON buffer (performance and stability improvement) (PR #2336)

#### Build 2112030

-   Fixed ESP32 crash on Colortwinkles brightness change
-   Fixed setting picker to black resetting hue and saturation
-   Fixed auto white mode not saved to config

#### Build 2111300

-   Added CCT and white balance correction support (PR #2285)
-   Unified UI slider style
-   Added LED settings config template upload

#### Build 2111220

-   Fixed preset cycle not working from preset called by UI
-   Reintroduced permanent min. and max. cycle bounds

#### Build 2111190

-   Changed default ESP32 LED pin from 16 to 2
-   Renamed "Running 2" to "Chase 2"
-   Renamed "Tri Chase" to "Chase 3"

#### Build 2111170

-   Version bump to 0.13.0-b5 "Toki"
-   Improv Serial support (PR #2334)
-   Button improvements (PR #2284)
-   Added two time zones (PR #2264, 2311)
-   JSON in/decrementing support for brightness and presets
-   Fixed no gamma correction for JSON individual LED control
-   Preset cycle bugfix
-   Removed ledCount
-   LED settings buffer bugfix
-   Network pin conflict bugfix
-   Changed default ESP32 partition layout to 4M, 1M FS

#### Build 2110110

-   Version bump to 0.13.0-b4 "Toki"
-   Added option for bus refresh if off (PR #2259)
-   New auto segment logic
-   Fixed current calculations for virtual or non-linear configs (PR #2262)

#### Build 2110060

-   Added virtual network DDP busses (PR #2245)
-   Allow playlist as end preset in playlist
-   Improved bus start field UX
-   Pin reservations improvements (PR #2214)

#### Build 2109220

-   Version bump to 0.13.0-b3 "Toki"
-   Added segment names (PR #2184)
-   Improved Police and other effects (PR #2184)
-   Reverted PR #1902 (Live color correction - will be implemented as usermod) (PR #2175)
-   Added transitions for segment on/off
-   Improved number of sparks/stars in Fireworks effect with low number of segments
-   Fixed segment name edit pencil disappearing with request
-   Fixed color transition active even if the segment is off
-   Disallowed file upload with OTA lock active
-   Fixed analog invert option missing (PR #2219)

#### Build 2109100

-   Added an auto create segments per bus setting
-   Added 15 new palettes from SR branch (PR #2134)
-   Fixed segment runtime not reset on FX change via HTTP API
-   Changed AsyncTCP dependency to pbolduc fork v1.2.0

#### Build 2108250

-   Added Sync groups (PR #2150)
-   Added JSON API over Serial support
-   Live color correction (PR #1902)

#### Build 2108180

-   Fixed JSON IR remote not working with codes greater than 0xFFFFFF (fixes #2135)
-   Fixed transition 0 edge case

#### Build 2108170

-   Added application level pong websockets reply (#2139)
-   Use AsyncTCP 1.0.3 as it mitigates the flickering issue from 0.13.0-b2
-   Fixed transition manually updated in preset overridden by field value

#### Build 2108050

-   Fixed undesirable color transition from Orange to boot preset color on first boot
-   Removed misleading Delete button on new playlist with one entry
-   Updated NeoPixelBus to 2.6.7 and AsyncTCP to 1.1.1

#### Build 2107230

-   Added skinning (extra custom CSS) (PR #2084)
-   Added presets/config backup/restore (PR #2084)
-   Added option for using length instead of Stop LED in UI (PR #2048)
-   Added custom `holidays.json` holiday list (PR #2048)

#### Build 2107100

-   Version bump to 0.13.0-b2 "Toki"
-   Accept hex color strings in individual LED API
-   Fixed transition property not applying unless power/bri/color changed next
-   Moved transition field below segments (temporarily)
-   Reduced unneeded websockets pushes

#### Build 2107091

-   Fixed presets using wrong call mode (e.g. causing buttons to send UDP under direct change type)
-   Increased hue buffer
-   Renamed `NOTIFIER_CALL_MODE_` to `CALL_MODE_`

#### Build 2107090

-   Busses extend total configured LEDs if required
-   Fixed extra button pins defaulting to 0 on first boot

#### Build 2107080

-   Made Peek use the main websocket connection instead of opening a second one
-   Temperature usermod fix (from @blazoncek's dev branch)

#### Build 2107070

-   More robust initial resource loading in UI
-   Added `getJsonValue()` for usermod config parsing (PR #2061)
-   Fixed preset saving over websocket
-   Alpha ESP32 S2 support (filesystem does not work) (PR #2067)

#### Build 2107042

-   Updated ArduinoJson to 6.18.1
-   Improved Twinkleup effect
-   Fixed preset immediately deselecting when set via HTTP API `PL=`

#### Build 2107041

-   Restored support for "PL=~" mistakenly removed in 2106300
-   JSON IR improvements

#### Build 2107040

-   Playlist entries are now more compact
-   Added the possibility to enter negative numbers for segment offset

#### Build 2107021

-   Added WebSockets support to UI

#### Build 2107020

-   Send websockets on every state change
-   Improved Aurora effect

#### Build 2107011

-   Added MQTT button feedback option (PR #2011)

#### Build 2107010

-   Added JSON IR codes (PR #1941)
-   Adjusted the width of WiFi and LED settings input fields
-   Fixed a minor visual issue with slider trail not reaching thumb on low values

#### Build 2106302

-   Fixed settings page broken by using "%" in input fields

#### Build 2106301

-   Fixed a problem with disabled buttons reverting to pin 0 causing conflict

#### Build 2106300

-   Version bump to 0.13.0-b0 "Toki"
-   BREAKING: Removed preset cycle (use playlists)
-   BREAKING: Removed `nl.fade`, `leds.pin` and `ccnf` from JSON API
-   Added playlist editor UI
-   Reordered segment UI and added offset field
-   Raised maximum MQTT password length to 64 (closes #1373)

#### Build 2106290

-   Added Offset to segments, allows shifting the LED considered first within a segment
-   Added `of` property to seg object in JSON API to set offset
-   Usermod settings improvements (PR #2043, PR #2045)

#### Build 2106250

-   Fixed preset only disabling on second effect/color change

#### Build 2106241

-   BREAKING: Added ability for usermods to force a config save if config incomplete. `readFromConfig()` needs to return a `bool` to indicate if the config is complete
-   Updated usermods implementing `readFromConfig()`
-   Auto-create segments based on configured busses

#### Build 2106200

-   Added 2 Ethernet boards and split Ethernet configs into separate file

#### Build 2106180

-   Fixed DOS on Chrome tab restore causing reboot

#### Build 2106170

-   Optimized JSON buffer usage (pre-serialized color arrays)

#### Build 2106140

-   Updated main logo
-   Reduced flash usage by 0.8kB by using 8-bit instead of 32-bit PNGs for welcome and 404 pages
-   Added a check to stop Alexa reporting an error if state set by macro differs from the expected state

#### Build 2106100

-   Added support for multiple buttons with various types (PR #1977)
-   Fixed infinite playlists (PR #2020)
-   Added `r` to playlist object, allows for shuffle regardless of the `repeat` value
-   Improved accuracy of NTP time sync
-   Added possibility for WLED UDP sync to sync system time
-   Improved UDP sync accuracy, if both sender and receiver are NTP synced
-   Fixed a cache issue with restored tabs
-   Cache CORS request
-   Disable WiFi sleep by default on ESP32

#### Build 2105230

-   No longer retain MQTT `/v` topic to alleviate storage loads on MQTT broker
-   Fixed Sunrise calculation (atan_t approx. used outside of value range)

#### Build 2105200

-   Fixed WS281x output on ESP32
-   Fixed potential out-of-bounds write in MQTT
-   Fixed IR pin not changeable if IR disabled
-   Fixed XML API <wv> containing -1 on Manual only RGBW mode (see #888, #1783)

#### Build 2105171

-   Always copy MQTT payloads to prevent non-0-terminated strings
-   Updated ArduinoJson to 6.18.0
-   Added experimental support for `{"on":"t"}` to toggle on/off state via JSON

#### Build 2105120

-   Fixed possibility of non-0-terminated MQTT payloads
-   Fixed two warnings regarding integer comparison

#### Build 2105112

-   Usermod settings page no usermods message
-   Lowered min speed for Drip effect

#### Build 2105111

-   Fixed various Codacy code style and logic issues

#### Build 2105110

-   Added Usermod settings page and configurable usermods (PR #1951)
-   Added experimental `/json/cfg` endpoint for changing settings from JSON (see #1944, not part of official API)

#### Build 2105070

-   Fixed not turning on after pressing "Off" on IR remote twice (#1950)
-   Fixed OTA update file selection from Android app (TODO: file type verification in JS, since android can't deal with accept='.bin' attribute)

#### Build 2104220

-   Version bump to 0.12.1-b1 "Hikari"
-   Release and build script improvements (PR #1844)

#### Build 2104211

-   Replace default TV simulator effect with the version that saves 18k of flash and appears visually identical

#### Build 2104210

-   Added `tb` to JSON state, allowing setting the timebase (set tb=0 to start e.g. wipe effect from the beginning). Receive only.
-   Slightly raised Solid mode refresh rate to work with LEDs (TM1814) that require refresh rates of at least 2fps
-   Added sunrise and sunset calculation to the backup JSON time source

#### Build 2104151

-   `NUM_STRIPS` no longer required with compile-time strip defaults
-   Further optimizations in wled_math.h

#### Build 2104150

-   Added ability to add multiple busses as compile time defaults using the esp32_multistrip usermod define syntax

#### Build 2104141

-   Reduced memory usage by 540b by switching to a different trigonometric approximation

#### Build 2104140

-   Added dynamic location-based Sunrise/Sunset macros (PR #1889)
-   Improved seasonal background handling (PR #1890)
-   Fixed instance discovery not working if MQTT not compiled in
-   Fixed Button, IR, Relay pin not assigned by default (resolves #1891)

#### Build 2104120

-   Added switch support (button macro is switch closing action, long press macro switch opening)
-   Replaced Circus effect with new Running Dual effect (Circus is Tricolor Chase with Red/White/Black)
-   Fixed ledmap with multiple segments (PR #1864)

#### Build 2104030

-   Fixed ESP32 crash on Drip effect with reversed segment (#1854)
-   Added flag `WLED_DISABLE_BROWNOUT_DET` to disable ESP32 brownout detector (off by default)

### WLED release 0.12.0

#### Build 2104020

-   Allow clearing button/IR/relay pin on platforms that don't support negative numbers
-   Removed AUX pin
-   Hid some easter eggs, only to be found at easter

### Development versions between 0.11.1 and 0.12.0 releases

#### Build 2103310

-   Version bump to 0.12.0 "Hikari"
-   Fixed LED settings submission in iOS app

#### Build 2103300

-   Version bump to 0.12.0-b5 "Hikari"
-   Update to core espressif32@3.2
-   Fixed IR pin not configurable

#### Build 2103290

-   Version bump to 0.12.0-b4 "Hikari"
-   Experimental use of espressif32@3.1.1
-   Fixed RGBW mode disabled after LED settings saved
-   Fixed infrared support not compiled in if IRPIN is not defined

#### Build 2103230

-   Fixed current estimation

#### Build 2103220

-   Version bump to 0.12.0-b2 "Hikari"
-   Worked around an issue causing a critical decrease in framerate (wled.cpp l.240 block)
-   Bump to Espalexa v2.7.0, fixing discovery

#### Build 2103210

-   Version bump to 0.12.0-b1 "Hikari"
-   More colors visible on Palette preview
-   Fixed chevron icon not included
-   Fixed color order override
-   Cleanup

#### Build 2103200

-   Version bump to 0.12.0-b0 "Hikari"
-   Added palette preview and search (PR #1637)
-   Added Reverse checkbox for PWM busses - reverses logic level for on
-   Fixed various problems with the Playlist feature (PR #1724)
-   Replaced "Layer" icon with "i" icon for Info button
-   Chunchun effect more fitting for various segment lengths (PR #1804)
-   Removed global reverse (in favor of individual bus reverse)
-   Removed some unused icons from UI icon font

#### Build 2103130

-   Added options for Auto Node discovery
-   Optimized strings (no string both F() and raw)

#### Build 2103090

-   Added Auto Node discovery (PR #1683)
-   Added tooltips to quick color selectors for accessibility

#### Build 2103060

-   Auto start field population in bus config

#### Build 2103050

-   Fixed incorrect over-memory indication in LED settings on ESP32

#### Build 2103041

-   Added destructor for BusPwm (fixes #1789)

#### Build 2103040

-   Fixed relay mode inverted when upgrading from 0.11.0
-   Fixed no more than 2 pins per bus configurable in UI
-   Changed to non-linear IR brightness steps (PR #1742)
-   Fixed various warnings (PR #1744)
-   Added UDP DNRGBW Mode (PR #1704)
-   Added dynamic LED mapping with ledmap.json file (PR #1738)
-   Added support for QuinLED-ESP32-Ethernet board
-   Added support for WESP32 ethernet board (PR #1764)
-   Added Caching for main UI (PR #1704)
-   Added Tetrix mode (PR #1729)
-   Removed Merry Christmas mode (use "Chase 2" - called Running 2 before 0.13.0)
-   Added memory check on Bus creation

#### Build 2102050

-   Version bump to 0.12.0-a0 "Hikari"
-   Added FPS indication in info
-   Bumped max outputs from 7 to 10 busses for ESP32

#### Build 2101310

-   First alpha configurable multipin

#### Build 2101130

-   Added color transitions for all segments and slots and for segment brightness
-   Fixed bug that prevented setting a boot preset higher than 25

#### Build 2101040

-   Replaced Red & Blue effect with Aurora effect (PR #1589)
-   Fixed HTTP changing segments uncommanded (#1618)
-   Updated copyright year and contributor page link

#### Build 2012311

-   Fixed Countdown mode

#### Build 2012310

-   (Hopefully actually) fixed display of usermod values in info screen

#### Build 2012240

-   Fixed display of usermod values in info screen
-   4 more effects now use FRAMETIME
-   Remove unsupported environments from platformio.ini

#### Build 2012210

-   Split index.htm in separate CSS + JS files (PR #1542)
-   Minify UI HTML, saving >1.5kB flash
-   Fixed JShint warnings

#### Build 2012180

-   Boot brightness 0 will now use the brightness from preset
-   Add iOS scrolling momentum (from PR #1528)

### WLED release 0.11.1

#### Build 2012180

-   Release of WLED 0.11.1 "Mirai"
-   Fixed AP hide not saving (fixes #1520)
-   Fixed MQTT password re-transmitted to HTML
-   Hide Update buttons while uploading, accept .bin
-   Make sure AP password is at least 8 characters long

### Development versions after 0.11.0 release

#### Build 2012160

-   Bump Espalexa to 2.5.0, fixing discovery (PR Espalexa/#152, originally PR #1497)

#### Build 2012150

-   Added Blends FX (PR #1491)
-   Fixed an issue that made it impossible to deactivate timed presets

#### Build 2012140

-   Added Preset ID quick display option (PR #1462)
-   Fixed LEDs not turning on when using gamma correct brightness and LEDPIN 2 (default)
-   Fixed notifier applying main segment to selected segments on notification with FX/Col disabled 

#### Build 2012130

-   Fixed RGBW mode not saved between reboots (fixes #1457)
-   Added brightness scaling in palette function for default (PR #1484)

#### Build 2012101

-   Fixed preset cycle default duration rounded down to nearest 10sec interval (#1458)
-   Enabled E1.31/DDP/Art-Net in AP mode

#### Build 2012100

-   Fixed multi-segment preset cycle
-   Fixed EEPROM (pre-0.11 settings) not cleared on factory reset
-   Fixed an issue with intermittent crashes on FX change (PR #1465)
-   Added function to know if strip is updating (PR #1466)
-   Fixed using colorwheel sliding the UI (PR #1459)
-   Fixed analog clock settings not saving (PR #1448)
-   Added Temperature palette (PR #1430)
-   Added Candy cane FX (PR #1445)

#### Build 2012020

-   UDP `parsePacket()` with sync disabled (#1390)
-   Added Multi RGBW DMX mode (PR #1383)

#### Build 2012010

-   Fixed compilation for analog (PWM) LEDs

### WLED version 0.11.0

#### Build 2011290

-   Release of WLED 0.11.0 "Mirai"
-   Workaround for weird empty %f Espalexa issue
-   Fixed crash on saving preset with HTTP API `PS`
-   Improved performance for color changes in non-main segment

#### Build 2011270

-   Added tooltips for speed and intensity sliders (PR #1378)
-   Moved color order to NpbWrapper.h
-   Added compile time define to override the color order for a specific range

#### Build 2011260

-   Add `live` property to state, allowing toggling of realtime (not incl. in state resp.)
-   PIO environment changes

#### Build 2011230

-   Version bump to 0.11.0 "Mirai"
-   Improved preset name sorting
-   Fixed Preset cycle not working beyond preset 16

### Development versions between 0.10.2 and 0.11.0 releases

#### Build 2011220

-   Fixed invalid save when modifying preset before refresh (might be related to #1361)
-   Fixed brightness factor ignored on realtime timeout (fixes #1363)
-   Fixed Phase and Chase effects with LED counts >256 (PR #1366)

#### Build 2011210

-   Fixed Brightness slider beneath color wheel not working (fixes #1360)
-   Fixed invalid UI state after saving modified preset

#### Build 2011200

-   Added HEX color receiving to JSON API with `"col":["RRGGBBWW"]` format
-   Moved Kelvin color receiving in JSON API from `"col":[[val]]` to `"col":[val]` format
    _Notice:_ This is technically a breaking change. Since no release was made since the introduction and the Kelvin property was not previously documented in the wiki,
    impact should be minimal. 
-   BTNPIN can now be disabled by setting to -1 (fixes #1237)

#### Build 2011180

-   Platformio.ini updates and streamlining (PR #1266)
-   my_config.h custom compile settings system (not yet used for much, adapted from PR #1266)
-   Added Hawaii timezone (HST)
-   Linebreak after 5 quick select buttons

#### Build 2011154

-   Fixed RGBW saved incorrectly
-   Fixed pmt caching requesting /presets.json too often
-   Fixed deEEP not copying the first segment of EEPROM preset 16

#### Build 2011153

-   Fixed an ESP32 end-of-file issue
-   Fixed strip.isRgbw not read from cfg.json

#### Build 2011152

-   Version bump to 0.11.0p "Mirai"
-   Increased max. num of segments to 12 (ESP8266) / 16 (ESP32)
-   Up to 250 presets stored in the `presets.json` file in filesystem
-   Complete overhaul of the Presets UI tab
-   Updated iro.js to v5 (fixes black color wheel)
-   Added white temperature slider to color wheel
-   Add JSON settings serialization/deserialization to cfg.json and wsec.json
-   Added deEEP to convert the EEPROM settings and presets to files
-   Playlist support - JSON only for now
-   New v2 usermod methods `addToConfig()` and `readFromConfig()` (see EXAMPLE_v2 for doc)
-   Added Ethernet support for ESP32 (PR #1316)
-   IP addresses are now handled by the `Network` class
-   New `esp32_poe` PIO environment
-   Use EspAsyncWebserver Aircoookie fork v.2.0.0 (hiding wsec.json)
-   Removed `WLED_DISABLE_FILESYSTEM` and `WLED_ENABLE_FS_SERVING` defines as they are now required
-   Added pin manager
-   UI performance improvements (no drop shadows)
-   More explanatory error messages in UI
-   Improved candle brightness
-   Return remaining nightlight time `nl.rem` in JSON API (PR #1302)
-   UI sends timestamp with every command, allowing for timed presets without using NTP
-   Added gamma calculation (yet unused)
-   Added LED type definitions to const.h (yet unused)
-   Added nicer 404 page
-   Removed `NP` and `MS=` macro HTTP API commands
-   Removed macros from Time settings

#### Build 2011120

-   Added the ability for the /api MQTT topic to receive JSON API payloads

#### Build 2011040

-   Inverted Rain direction (fixes #1147)

#### Build 2011010

-   Re-added previous C9 palette
-   Renamed new C9 palette

#### Build 2010290

-   Colorful effect now supports palettes
-   Added C9 2 palette (#1291)
-   Improved C9 palette brightness by 12%
-   Disable onboard LED if LEDs are off (PR #1245)
-   Added optional status LED (PR #1264)
-   Realtime max. brightness now honors brightness factor (fixes #1271)
-   Updated ArduinoJSON to 6.17.0

#### Build 2010020

-   Fixed interaction of `T` and `NL` HTTP API commands (#1214)
-   Fixed an issue where Sunrise mode nightlight does not activate if toggled on simultaneously 

#### Build 2009291

-   Fixed MQTT bootloop (no F() macro, #1199)

#### Build 2009290

-   Added basic DDP protocol support
-   Added Washing Machine effect (PR #1208)

#### Build 2009260

-   Added Loxone parser (PR #1185)
-   Added support for kelvin input via `K=` HTTP and `"col":[[val]]` JSON API calls
    _Notice:_ `"col":[[val]]` removed in build 2011200, use `"col":[val]`
-   Added supplementary UDP socket (#1205)
-   TMP2.net receivable by default
-   UDP sockets accept HTTP and JSON API commands
-   Fixed missing timezones (#1201)

#### Build 2009202

-   Fixed LPD8806 compilation

#### Build 2009201

-   Added support for preset cycle toggling using CY=2
-   Added ESP32 touch pin support (#1190)
-   Fixed modem sleep on ESP8266 (#1184)

#### Build 2009200

-   Increased available heap memory by 4kB
-   Use F() macro for the majority of strings
-   Restructure timezone code
-   Restructured settings saved code
-   Updated ArduinoJSON to 6.16.1

#### Build 2009170

-   New WLED logo on Welcome screen (#1164)
-   Fixed 170th pixel dark in E1.31

#### Build 2009100

-   Fixed sunrise mode not reinitializing
-   Fixed passwords not clearable

#### Build 2009070

-   New Segments are now initialized with default speed and intensity

#### Build 2009030

-   Fixed bootloop if mDNS is used on builds without OTA support

### WLED version 0.10.2

#### Build 2008310

-   Added new logo
-   Maximum GZIP compression (#1126)
-   Enable WebSockets by default

### Development versions between 0.10.0 and 0.10.2 releases

#### Build 2008300

-   Added new UI customization options to UI settings
-   Added Dancing Shadows effect (#1108)
-   Preset cycle is now paused if lights turned off or nightlight active
-   Removed `esp01` and `esp01_ota` envs from travis build (need too much flash)

#### Build 2008290

-   Added individual LED control support to JSON API
-   Added internal Segment Freeze/Pause option

#### Build 2008250

-   Made `platformio_override.ini` example easier to use by including the `default_envs` property
-   FastLED uses `now` as timer, so effects using e.g. `beatsin88()` will sync correctly
-   Extended the speed range of Pacifica effect
-   Improved TPM2.net receiving (#1100)
-   Fixed exception on empty MQTT payload (#1101)

#### Build 2008200

-   Added segment mirroring to web UI
-   Fixed segment mirroring when in reverse mode

#### Build 2008140

-   Removed verbose live mode info from `<ds>` in HTTP API response

#### Build 2008100

-   Fixed Auto White mode setting (fixes #1088)

#### Build 2008070

-   Added segment mirroring (`mi` property) (#1017)
-   Fixed DMX settings page not displayed (#1070)
-   Fixed ArtNet multi universe and improve code style (#1076)
-   Renamed global var `local` to `localTime` (#1078)

#### Build 2007190

-   Fixed hostname containing illegal characters (#1035)

#### Build 2006251

-   Added `SV=2` to HTTP API, allow selecting single segment only

#### Build 2006250

-   Fix Alexa not turning off white channel (fixes #1012)

#### Build 2006220

-   Added Sunrise nightlight mode
-   Added Chunchun effect
-   Added `LO` (live override) command to HTTP API
-   Added `mode` to `nl` object of JSON state API, deprecating `fade`
-   Added light color scheme support to web UI (click sun next to brightness slider)
-   Added option to hide labels in web UI (click flame icon next to intensity slider)
-   Added hex color input (click palette icon next to palette select) (resolves #506)
-   Added support for RGB sliders (need to set in localstorage)
-   Added support for custom background color or image (need to set in localstorage)
-   Added option to hide bottom tab bar in PC mode (need to set in localstorage)
-   Fixed transition lag with multiple segments (fixes #985)
-   Changed Nightlight wording (resolves #940)

#### Build 2006060

-   Added five effects by Andrew Tuline (Phased, Phased Noise, Sine, Noise Pal and Twinkleup)
-   Added two new effects by Aircoookie (Sunrise and Flow)
-   Added US-style sequence to traffic light effect
-   Merged pull request #964 adding 9 key IR remote

#### Build 2005280

-   Added v2 usermod API
-   Added v2 example usermod `usermod_v2_example` in the usermods folder as prelimary documentation
-   Added DS18B20 Temperature usermod with Info page support
-   Disabled MQTT on ESP01 build to make room in flash

#### Build 2005230

-   Fixed TPM2

#### Build 2005220

-   Added TPM2.NET protocol support (need to set WLED broadcast UDP port to 65506)
-   Added TPM2 protocol support via Serial
-   Support up to 6553 seconds preset cycle durations (backend, NOT yet in UI)
-   Merged pull request #591 fixing WS2801 color order
-   Merged pull request #858 adding fully featured travis builds
-   Merged pull request #862 adding DMX proxy feature

#### Build 2005100

-   Update to Espalexa v2.4.6 (+1.6kB free heap memory)
-   Added `m5atom` PlatformIO environment

#### Build 2005090

-   Default to ESP8266 Arduino core v2.7.1 in PlatformIO
-   Fixed Preset Slot 16 always indicating as empty (#891)
-   Disabled Alexa emulation by default (causes bootloop for some users)
-   Added BWLT11 and SHOJO_PCB defines to NpbWrapper
-   Merged pull request #898 adding Solid Glitter effect

### WLED version 0.10.0

#### Build 2005030

-   DMX Single RGW and Single DRGB modes now support an additional white channel
-   Improved palettes derived from set colors and changed their names

### Development versions between 0.9.1 and 0.10.0 release

#### Build 2005020

-   Added ACST and ACST/ACDT timezones

#### Build 2005010

-   Added module info page to web UI
-   Added realtime override functionality to web UI
-   Added individual segment power and brightness to web UI
-   Added feature to one-click select single segment only by tapping segment name
-   Removed palette jumping to default if color is changed

#### Build 2004300

-   Added realtime override option and `lor` JSON property
-   Added `lm` (live mode) and `lip` (live IP) properties to info in JSON API
-   Added reset commands to APIs
-   Added `json/si`, returning state and info, but no FX or Palette lists
-   Added rollover detection to millis(). Can track uptimes longer than 49 days
-   Attempted to fix Wifi issues with Unifi brand APs

#### Build 2004230

-   Added brightness and power for individual segments
-   Added `on` and `bri` properties to Segment object in JSON API
-   Added `C3` an `SB` commands to HTTP get API
-   Merged pull request #865 for 5CH_Shojo_PCB environment

#### Build 2004220

-   Added Candle Multi effect
-   Added Palette capability to Pacifica effect

#### Build 2004190

-   Added TM1814 type LED defines

#### Build 2004120

-   Added Art-Net support
-   Added OTA platform to platformio.ini

#### Build 2004100

-   Fixed DMX output compilation
-   Added DMX start LED setting

#### Build 2004061

-   Fixed RBG and BGR getPixelColor (#825)
-   Improved formatting

#### Build 2004060

-   Consolidated global variables in wled.h

#### Build 2003300

-   Major change of project structure from .ino to .cpp and func_declare.h

#### Build 2003262

-   Fixed compilation for Analog LEDs
-   Fixed sync settings network port fields too small

#### Build 2003261

-   Fixed live preview not displaying whole light if over 255 LEDs

#### Build 2003251

-   Added Pacifica effect (tentative, doesn't yet support other colors)
-   Added Atlantica palette
-   Fixed ESP32 build of Espalexa

#### Build 2003222

-   Fixed Alexa Whites on non-RGBW lights (bump Espalexa to 2.4.5)

#### Build 2003221

-   Moved Cronixie driver from FX library to drawOverlay handler

#### Build 2003211

-   Added custom mapping compile define to FX_fcn.h
-   Merged pull request #784 by @TravisDean: Fixed initialization bug when toggling skip first
-   Added link to youtube videos by Room31 to readme

#### Build 2003141

-   Fixed color of main segment returned in JSON API during transition not being target color (closes #765)
-   Fixed arlsLock() being called after pixels set in E1.31 (closes #772)
-   Fixed HTTP API calls not having an effect if no segment selected (now applies to main segment)

#### Build 2003121

-   Created changelog.md - make tracking changes to code easier
-   Merged pull request #766 by @pille: Fix E1.31 out-of sequence detection

