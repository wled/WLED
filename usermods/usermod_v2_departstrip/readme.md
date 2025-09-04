# DepartStrip

Render upcoming departures from SIRI StopMonitoring feeds on WLED LED
segments, keyed by `AGENCY:StopCode`.

## SIRI Basics

- What is SIRI? SIRI stands for “Service Interface for Real Time
  Information” — a CEN/Transmodel family standard for exchanging
  real‑time public transport data. StopMonitoring is the SIRI service
  that returns predicted arrivals/departures at a stop.

## Feed Examples: 511.org (SF Bay Area) and MTA (New York)

- 511.org StopMonitoring (Bay Area)
  - Endpoint: `http://api.511.org/transit/StopMonitoring?format=json&api_key=KEY&agency=AGENCY&stopCode=STOP`
  - Agencies include BART (`BA`), AC Transit (`AC`), and many others across the nine‑county region.

- MTA Bus Time StopMonitoring (NYC)
  - Endpoint: `http://bustime.mta.info/api/siri/stop-monitoring.json?key=KEY&OperatorRef=MTA&MonitoringRef=STOP`
  - Coverage: the New York City bus network (hundreds of routes
    including local, limited, SBS, and express).

- Where else can SIRI work?
  - SIRI is widely used in Europe (it’s a European CEN standard) and
    by a number of North American integrators. If your operator or
    regional aggregator mentions “SIRI” or “SIRI StopMonitoring” in
    their developer docs, you can likely adapt it here by setting the
    TemplateUrl and parameters.

## Configuration (WLED → Config → Usermods → DepartStrip)

- DepartStrip.Enabled — master enable.

Source sections (siri_source1, siri_source2, …)
- Enabled — enable/disable this source.
- UpdateSecs — fetch interval seconds (default 60).
- TemplateUrl — URL template with placeholders (see below):
  `http://api.511.org/transit/StopMonitoring?format=json&api_key={apikey}&agency={agency}&stopCode={stopcode}`
- ApiKey — substituted into `{apikey}`.
- AgencyStopCode — `AGENCY:StopCode` (e.g., `AC:50958`, `BA:900201`).

View sections (e.g., `DepartureView_AC:50958`, `DepartureView_BA:900201`)
- SegmentId — segment index to render (set `-1` to disable the view).
- AgencyStopCodes — one or more stop keys. Accepts full
  `AGENCY:StopCode` tokens or a leading full token followed by
  additional stop codes from the same agency, separated by
  comma/space. Examples: `AC:50958`, `AC:50958,50959`, `BA:900201
  900202`.

### URL Templates and Placeholders

Each source uses a `TemplateUrl` with placeholders replaced at fetch time:
- `{agency}` or `{AGENCY}` → agency code (e.g., `AC`, `BA`, `MTA`).
- `{stopcode}` or `{stopCode}` → stop code (e.g., `50958`).
- `{apikey}`, `{apiKey}`, or `{APIKEY}` → your API key.

If placeholders are present they are replaced directly. If none are
present, the provided URL is used as‑is (no legacy query parameters
are appended).

Examples
- 511.org:
  `http://api.511.org/transit/StopMonitoring?format=json&api_key={apikey}&agency={agency}&stopCode={stopcode}`
- MTA Bus Time:
  `http://bustime.mta.info/api/siri/stop-monitoring.json?key={apikey}&OperatorRef=MTA&MonitoringRef={stopcode}`

## Notes & Tips

- Colors: The colors for each line in an agencies feed may be set in
  the ColorMap section at the bottom.
- Rate limits: If you exceed a feed’s rate limit, you’ll see HTTP 429
  and a per‑source backoff message in the debug log. Each source backs
  off independently.
