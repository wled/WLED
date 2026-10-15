# Music Assistant usermod

Ties WLED's built-in "Image" effect to the cover art of whatever is currently
playing on a [Music Assistant](https://music-assistant.io) server. Poll a
configured player queue on a timer; when the track's cover art changes, fetch
it from Music Assistant's `/imageproxy` endpoint and feed it to the core
"Image" effect's live-render path.

## Requirements

- ESP32 (JPEG decoding is not available on ESP8266).
- Core must be built with both `-D WLED_ENABLE_GIF` and `-D WLED_ENABLE_JPEG`
  (the `esp32_all_variants` PlatformIO environment already sets both).
- A **2D** segment running the **Image** effect with its **Live** checkbox
  ticked (the live-render path requires a matrix; unlike the .gif path it
  does not support 1D strips). This usermod never changes effects or
  segments itself unless "Auto switch effect" is enabled - it only keeps the
  live cover-art buffer updated.

## Usage

```ini
[env:esp32_musicassistant]
extends = env:esp32dev
custom_usermods = ${env:esp32dev.custom_usermods} musicassistant
```

## Settings

### Enabled
Turns polling on/off.

### Host / Port
Address of your Music Assistant server, e.g. `mass` / `8095`.

### Token
A Bearer token for the Music Assistant HTTP API. Music Assistant requires
authentication for all API calls since API schema v28. Create a long-lived
token with the `auth/token/create` command (WebSocket or `POST /api`), or log
in via `POST /auth/login` and use the returned short-lived token. This token
is stored in `cfg.json`, same as any other usermod credential.

### Queue ID
The `queue_id` of the player/zone to track (a server can have many). Find it
via the `player_queues/all` command - it's the currently-active queue you
want reflected on your LEDs.

### Poll interval (ms)
How often to check for a track change. The queue-state poll is small and
cheap; the (larger) cover art JPEG is only fetched when the track actually
changes.

### Image size
Must be one of Music Assistant's supported `/imageproxy` sizes: `80`, `160`,
`256`, `512`, `1024` (or `0` for original size - not recommended, can be
large). `80` is plenty for most LED matrices.

### Auto switch effect
When enabled, the usermod forces the configured segment into the "Image"
effect with "Live" ticked whenever playback starts and a cover art image is
available. It only acts on that start-of-playback transition - it won't
fight you if you manually switch away to another effect while still playing.
Off by default: the usermod otherwise never touches segment/effect selection.

### Segment ID
Which segment "Auto switch effect" controls. Ignored if that option is off.
