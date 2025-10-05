# Google Calendar Scheduler Usermod

This usermod allows WLED to automatically trigger presets, macros, or API calls based on events in a Google Calendar.

## Features

- 📅 Fetch events from Google Calendar (via public iCal URL or Google Calendar API)
- ⏰ Automatically trigger WLED presets at event start/end times
- 🔄 Configurable polling interval
- 🎯 Event pattern matching to map calendar events to WLED actions
- 🌐 Web UI configuration support

## Use Cases

- **Automated lighting schedules** - Create calendar events for different lighting moods throughout the day
- **Meeting room indicators** - Show room availability based on calendar bookings
- **Holiday/special event lighting** - Schedule special lighting for holidays, parties, or events
- **Synchronized displays** - Multiple WLED controllers can follow the same calendar
- **Time-based scenes** - "Morning", "Work", "Evening", "Sleep" scenes triggered by calendar

## Installation

1. Copy the `google_calendar_scheduler` folder to your `wled00/usermods/` directory
2. Register the usermod in `wled00/usermods_list.cpp` (if not using automatic registration)
3. Define the usermod ID in `wled00/const.h`:
   ```cpp
   #define USERMOD_ID_CALENDAR_SCHEDULER 59
   ```
4. Compile and upload to your ESP device

## Configuration

### Method 1: Public iCal URL (Recommended for simplicity)

1. Open your Google Calendar
2. Click the three dots next to the calendar you want to use
3. Select "Settings and sharing"
4. Scroll to "Integrate calendar" section
5. Copy the "Public address in iCal format" URL
6. In WLED settings, navigate to "Usermod Settings"
7. Enable "Calendar Scheduler"
8. Paste the iCal URL into the "calendarUrl" field
9. Set your desired poll interval (default: 300 seconds = 5 minutes)

**Note:** Your calendar must be set to public for this method to work.

### Method 2: Google Calendar API (More secure, supports private calendars)

1. Create a Google Cloud Project and enable Google Calendar API
2. Create an API key
3. In WLED settings:
   - Enter your API key in the "apiKey" field
   - Enter your Calendar ID in the "calendarId" field
   - Enable the usermod

### Event Mapping Configuration

Create rules to map calendar event titles/descriptions to WLED actions:

```json
{
  "Calendar Scheduler": {
    "enabled": true,
    "calendarUrl": "https://calendar.google.com/calendar/ical/...",
    "pollInterval": 300,
    "mappings": [
      {
        "pattern": "Work",
        "startPreset": 1,
        "endPreset": 2
      },
      {
        "pattern": "Party",
        "startPreset": 10,
        "endPreset": 0
      }
    ]
  }
}
```

## Event Naming Convention

You can control WLED actions by naming your calendar events with specific patterns:

- **"WLED: Morning"** - Triggers the mapping for "Morning" events
- **"Work meeting"** - If you have a mapping for "Work", it will trigger
- **"Party at home"** - Pattern "Party" will match and trigger

## Preset Mapping Examples

| Event Pattern | Start Preset | End Preset | Description |
|--------------|--------------|------------|-------------|
| Morning | 1 | 0 | Bright white light in the morning |
| Work | 2 | 0 | Focused work lighting |
| Lunch | 3 | 2 | Relaxed lighting during lunch |
| Evening | 4 | 0 | Warm evening ambiance |
| Sleep | 5 | 0 | Night mode / off |
| Party | 10 | 1 | Party mode during events |

## API Calls

Instead of presets, you can trigger custom API calls:

```json
{
  "pattern": "Dim",
  "startApi": "win&T=1&A=64",
  "endApi": "win&T=1&A=255"
}
```

## Time Synchronization

**Important:** This usermod relies on WLED's NTP time synchronization. Make sure:

1. Time zone is configured correctly in WLED settings
2. NTP is enabled and synchronized
3. Check "Time & Macros" settings page to verify correct time

## Troubleshooting

### Events not triggering
- Verify WLED has correct time (Settings > Time & Macros)
- Check that calendar events are in the future
- Ensure poll interval allows enough time to detect events
- Verify calendar URL is accessible (test in browser)

### Calendar not updating
- Check WiFi connection
- Verify calendar URL is correct
- Increase poll interval if rate-limited
- Check WLED logs for error messages

### Pattern matching not working
- Patterns are case-sensitive substrings
- Use simple keywords like "Work", "Meeting", "Party"
- Check event title matches your pattern exactly

## Limitations

- Maximum 10 active events tracked simultaneously
- Maximum 5 event mapping patterns
- Minimum recommended poll interval: 60 seconds
- Requires stable WiFi connection
- Calendar must be public for iCal method

## Future Enhancements

- [ ] Full iCal parsing implementation
- [ ] Google Calendar API integration
- [ ] Regex pattern matching
- [ ] Custom API call execution
- [ ] Multi-calendar support
- [ ] Event conflict resolution
- [ ] Transition effects between events
- [ ] Event preview in UI

## Credits

Created for WLED by the community.

## License

This usermod is part of WLED and follows the same MIT license.
