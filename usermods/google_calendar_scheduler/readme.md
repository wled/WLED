# Google Calendar Scheduler Usermod

This usermod allows WLED to automatically trigger presets or API calls based on events in a Google Calendar.

## Features

- 📅 Fetch events from Google Calendar via public or secret iCal URL
- ⏰ Automatically trigger WLED presets or JSON API commands when events start
- 🔄 Configurable polling interval (default: 5 minutes)
- 🎯 Two execution modes: Preset name matching or direct JSON API
- 🌐 Full web UI configuration support
- 🔒 Supports Google Calendar "secret address" for private calendars

## Use Cases

- **Automated lighting schedules** - Create calendar events for different lighting moods throughout the day
- **Meeting room indicators** - Show room availability based on calendar bookings
- **Holiday/special event lighting** - Schedule special lighting for holidays, parties, or events
- **Synchronized displays** - Multiple WLED controllers can follow the same calendar
- **Time-based scenes** - "Morning", "Work", "Evening", "Sleep" scenes triggered by calendar

## Installation

### PlatformIO

Add to your `platformio_override.ini`:

```ini
[env:my_build]
extends = env:esp32dev
build_flags = ${env:esp32dev.build_flags}
  -D USERMOD_ID_CALENDAR_SCHEDULER=59
custom_usermods = google_calendar_scheduler
```

### Manual Installation

1. Copy the `google_calendar_scheduler` folder to your `wled00/usermods/` directory
2. Add to `wled00/const.h`:
   ```cpp
   #define USERMOD_ID_CALENDAR_SCHEDULER 59
   ```
3. Compile and upload to your ESP device

## Configuration

### Step 1: Get Your Google Calendar iCal URL

#### Option A: Public Calendar (Simple)
1. Open [Google Calendar](https://calendar.google.com/)
2. Click the three dots next to the calendar you want to use
3. Select "Settings and sharing"
4. Under "Access permissions", check "Make available to public"
5. Scroll to "Integrate calendar" section
6. Copy the "Public address in iCal format" URL

#### Option B: Secret Address (Private Calendar)
1. Open [Google Calendar](https://calendar.google.com/)
2. Click the three dots next to the calendar
3. Select "Settings and sharing"
4. Scroll to "Integrate calendar" section
5. Copy the "Secret address in iCal format" URL
   - This URL contains a unique token
   - Calendar remains private, but anyone with the URL can view events
   - You can reset the secret URL if needed

### Step 2: Configure WLED

1. In WLED web interface, go to **Config** > **Usermods**
2. Find "Calendar Scheduler"
3. Enable the usermod
4. Paste your iCal URL into the "calendarUrl" field
5. Set poll interval in seconds (default: 300 = 5 minutes)
6. Save settings

## How It Works

The usermod fetches your calendar events and executes actions when an event starts:

### Mode 1: Preset Name Matching

Put the **preset name** in the event description:

```
Event Title: Morning Routine
Event Description: Bright Morning
```

When this event starts, WLED will search for a preset named "Bright Morning" (case-insensitive) and apply it.

**Creating Presets:**
1. Configure your desired WLED state (colors, effects, etc.)
2. Go to **Presets** and save with a descriptive name
3. Use that exact name in your calendar event description

### Mode 2: JSON API Commands

Put **JSON API commands** in the event description for advanced control:

```
Event Title: Custom Lighting
Event Description: {"on":true,"bri":200,"seg":[{"col":[[255,0,0]]}]}
```

This allows full control over WLED state. You can:
- Turn on/off: `{"on":false}`
- Set brightness: `{"bri":128}`
- Change colors: `{"seg":[{"col":[[255,100,0]]}]}`
- Apply effects: `{"seg":[{"fx":12}]}`
- And more - see [WLED JSON API docs](https://kno.wled.ge/interfaces/json-api/)

**Tip:** Set up your desired state in WLED, then check `/json/state` to see the JSON you need.

## Example Calendar Events

| Event Description | What Happens |
|-------------------|--------------|
| `Off` | Applies preset named "Off" (turns off lights) |
| `Morning Bright` | Applies preset named "Morning Bright" |
| `{"on":false}` | Turns off via JSON API |
| `{"on":true,"bri":255}` | Full brightness via JSON API |
| `{"seg":[{"fx":9,"sx":128}]}` | Sets effect #9 with speed 128 |

## Calendar Event Tips

1. **Event Duration**: Actions trigger when event **starts**. Event end time is tracked but doesn't trigger actions currently.

2. **Recurring Events**: Works great! Set up "Morning" at 7 AM daily, "Evening" at 6 PM daily, etc.

3. **All-day Events**: Triggers at midnight in your timezone

4. **Time Zones**: Make sure WLED's timezone matches your calendar (Config > Time & Macros)

5. **Multiple Presets**: You can have different events triggering different presets throughout the day

## Example Daily Schedule

Create these recurring calendar events:

| Time | Event Title | Description | Action |
|------|-------------|-------------|---------|
| 7:00 AM | Wake Up | Morning Bright | Preset: Warm white, 100% |
| 9:00 AM | Work Start | Focus Mode | Preset: Cool white, 80% |
| 12:00 PM | Lunch Break | Relax | Preset: Warm colors, 60% |
| 6:00 PM | Evening | Sunset | Preset: Orange/red gradient |
| 10:00 PM | Sleep | Off | Preset: Lights off |

## Time Synchronization

**Critical:** This usermod requires accurate time via NTP.

1. Go to **Config** > **Time & Macros**
2. Enable NTP
3. Set your correct timezone
4. Verify the displayed time is correct

If time is wrong, events won't trigger at the right moment!

## Troubleshooting

### Events not triggering

- ✅ Check WLED time is correct (**Config** > **Time & Macros**)
- ✅ Verify calendar URL in browser - should download an .ics file
- ✅ Check event description matches preset name exactly (case-insensitive)
- ✅ For JSON mode, validate JSON syntax at [jsonlint.com](https://jsonlint.com/)
- ✅ Check **Config** > **Usermods** shows "Events loaded: X" with X > 0

### Calendar not updating

- ✅ Check WiFi connection is stable
- ✅ Verify calendarUrl is correct (paste in browser to test)
- ✅ Try increasing poll interval if you have many events
- ✅ Check serial monitor for debug messages (if compiled with debug enabled)

### Preset name not found

- ✅ Preset name must match exactly (spaces, capitalization doesn't matter)
- ✅ Go to **Presets** page to see all preset names
- ✅ Try a simple test: Create preset "Test", create calendar event with description "test"

### High memory usage

- Reduce poll interval to fetch less frequently
- Limit calendar to only upcoming events (delete past events)
- The usermod tracks max 5 concurrent events

## Technical Details

- **Max Events**: 5 simultaneous events tracked
- **Max Preset Name Length**: Limited by WLED preset system
- **JSON Buffer Size**: Uses WLED's standard `JSON_BUFFER_SIZE` (32KB on ESP32, 24KB on ESP8266)
- **Poll Interval**: Recommended 60-300 seconds (1-5 minutes)
- **HTTPS Support**: Yes, uses WiFiClientSecure with `setInsecure()`
- **Certificate Validation**: Skipped (for Google Calendar compatibility)

## Security Notes

- **Public Calendar**: Anyone can view your events
- **Secret URL**: Secure enough for most use cases, but can be regenerated if compromised
- **No API Keys**: Uses iCal format, no Google API authentication needed
- **HTTPS**: Traffic is encrypted but certificate validation is disabled for compatibility

## Limitations

- Only event start time triggers actions (end time is tracked but not used)
- No recurring event expansion (Google Calendar API handles this server-side)
- Maximum 5 events loaded at once (oldest events dropped if exceeded)
- Requires stable WiFi connection
- Minimum recommended poll interval: 60 seconds (to avoid rate limiting)

## Future Enhancements

Potential features for future versions:
- Event end time actions
- Multiple calendar support
- Conditional logic (if event A and not event B)
- Transition duration settings
- Local event storage/caching
- Webhook support for instant updates

## Credits

Created for the WLED community.

## License

MIT License - Part of the WLED project.
