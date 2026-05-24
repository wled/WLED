#pragma once
/*
 * UsermodHSVTicker - WLED Usermod v2
 * Fußball-Liveticker mit OpenLigaDB API
 * ESP32 DevKit v1 Type C, WLED 0.15.3
 *
 * Preset-Konzept:
 *  < 149  → Code pausiert, Farben bleiben
 *  149    → Manueller Refresh (sofortiger Neu-Abruf)
 *  150    → Offline (statisch, kein Polling)
 *  151    → Tor! (Solid Grün, 60s)
 *  152    → Gegentor (Solid Rot, 60s)
 *  153    → Deutschland (dfb → wm → em)
 *  154-189→ Bundesliga (auto bl1 → bl2 → bl3)
 *  200-223→ Premier League (auto pl → championship)
 *  230-231→ La Liga
 *  240-241→ Ligue 1
 *
 * Zeitplanung:
 *  - Täglich 06:00 UTC: Spielzeiten des Tages laden
 *  - 5 Min vor Spielbeginn: Polling startet (alle 30s)
 *  - Nach Spielende: Polling stoppt
 *  - Preset 149: sofortiger Refresh
 *
 * Zeitvergleich immer in UTC (OpenLigaDB liefert UTC)
 * WLED NTP muss aktiviert sein (Standard: pool.ntp.org)
 */

#include "wled.h"

#ifdef ARDUINO_ARCH_ESP32
  #include <HTTPClient.h>
  #include <freertos/semphr.h>
#endif

// ============================================================
// Preset-IDs
// ============================================================
#define PRESET_MIN_ACTIVE     149   // ab hier ist Code aktiv
#define PRESET_REFRESH        149
#define PRESET_OFFLINE        150
#define PRESET_GOAL_MINE      151
#define PRESET_GOAL_OPPONENT  152
#define PRESET_GERMANY        153
#define PRESET_BL_START       154
#define PRESET_BL_END         189
#define PRESET_PL_START       200
#define PRESET_PL_END         223
#define PRESET_LALIGA_START   230
#define PRESET_LALIGA_END     231
#define PRESET_LIGUE1_START   240
#define PRESET_LIGUE1_END     241

// ============================================================
// Liga-Kürzel pro Preset-Range
// Jede Liga hat eine Fallback-Kette
// ============================================================
struct LeagueChain {
  const char* leagues[4];  // bis zu 4 Fallback-Ligen, "" = Ende
  int         season;      // Saison-Jahr
};

// Gibt die Liga-Kette für ein Preset zurück
static LeagueChain getLeagueChain(uint8_t presetId) {
  if (presetId == PRESET_GERMANY)
    return {{"dfb", "wm", "em", ""}, 2025};
  if (presetId >= PRESET_BL_START && presetId <= PRESET_BL_END)
    return {{"bl1", "bl2", "bl3", ""}, 2025};
  if (presetId >= PRESET_PL_START && presetId <= PRESET_PL_END)
    return {{"pl", "championship", "", ""}, 2025};
  if (presetId >= PRESET_LALIGA_START && presetId <= PRESET_LALIGA_END)
    return {{"laliga", "laliga2", "", ""}, 2025};
  if (presetId >= PRESET_LIGUE1_START && presetId <= PRESET_LIGUE1_END)
    return {{"ligue1", "ligue2", "", ""}, 2025};
  return {{"", "", "", ""}, 2025};
}

// ============================================================
// Vereinsdatenbank (PROGMEM)
// Suchstring → Preset-ID
// ============================================================
struct ClubEntry {
  char    key[22];
  uint8_t presetId;
};

static const ClubEntry ALL_CLUBS[] PROGMEM = {
  // Bundesliga 154-189
  {"hamburg",       154},
  {"fuerth",        155}, {"greuther",    155}, {"greuter",     155},
  {"bayern",        156},
  {"dortmund",      157},
  {"leverkusen",    158},
  {"leipzig",       159},
  {"frankfurt",     160},
  {"wolfsburg",     161},
  {"freiburg",      162},
  {"augsburg",      163},
  {"mainz",         164},
  {"hoffenheim",    165},
  {"gladbach",      166},
  {"koeln",         167},
  {"union",         168},
  {"bochum",        169},
  {"heidenheim",    170},
  {"pauli",         171},
  {"kiel",          172},
  {"hertha",        173},
  {"schalke",       174},
  {"kaisers",       175},
  {"nuernberg",     176},
  {"darmstadt",     177},
  {"hannover",      178},
  {"duesseldorf",   179},
  {"magdeburg",     180},
  {"braunschweig",  181},
  {"karlsruhe",     182},
  {"dresden",       183},
  {"paderborn",     184},
  {"elversberg",    185},
  {"regensburg",    186},
  {"bielefeld",     187},
  {"saarb",         188},
  {"ulm",           189},
  // Premier League 200-223
  {"arsenal",       200},
  {"chelsea",       201},
  {"liverpool",     202},
  {"man city",      203}, {"manchester c", 203},
  {"man utd",       204}, {"manchester u", 204},
  {"tottenham",     205}, {"hotspur",      205},
  {"newcastle",     206},
  {"aston villa",   207},
  {"brighton",      208},
  {"west ham",      209},
  {"brentford",     210},
  {"fulham",        211},
  {"everton",       212},
  {"nottingham",    213},
  {"wolves",        214}, {"wolverhampton",214},
  {"crystal",       215},
  {"bournemouth",   216},
  {"leicester",     217},
  {"ipswich",       218},
  {"southampton",   219},
  {"leeds",         220},
  {"sunderland",    221},
  {"burnley",       222},
  {"sheffield",     223},
  // La Liga 230-231
  {"barcelona",     230},
  {"real madrid",   231}, {"madrid",       231},
  // Ligue 1 240-241
  {"paris",         240}, {"psg",          240},
  {"toulouse",      241},
};
static const int ALL_CLUBS_SIZE = sizeof(ALL_CLUBS) / sizeof(ClubEntry);

// Teamname → Preset-ID
static uint8_t findPresetForTeam(const String& name) {
  String lower = name; lower.toLowerCase();
  ClubEntry e;
  for (int i = 0; i < ALL_CLUBS_SIZE; i++) {
    memcpy_P(&e, &ALL_CLUBS[i], sizeof(ClubEntry));
    if (lower.indexOf(e.key) >= 0) return e.presetId;
  }
  return 0;
}

// ============================================================
// Spielzustand
// ============================================================
enum class MatchState : uint8_t { IDLE=0, SCHEDULED, LIVE, FINISHED };

struct MatchInfo {
  bool     valid        = false;
  bool     isLive       = false;
  bool     isFinished   = false;
  uint8_t  homeScore    = 0;
  uint8_t  awayScore    = 0;
  uint32_t kickoffUTC   = 0;   // Unix-Timestamp UTC
  char     homeTeam[48] = "";
  char     awayTeam[48] = "";
  char     league[16]   = "";  // in welcher Liga gefunden
};

// ============================================================
// Hauptklasse
// ============================================================
class UsermodHSVTicker : public Usermod {
private:

  // ---------- Konfiguration ----------
  bool _enabled        = true;

  // ---------- Zustand ----------
  uint8_t    _activePreset   = 0;     // aktuell gewähltes Preset
  uint8_t    _prevPreset     = 0;     // letztes Preset vor Refresh
  uint8_t    _teamPreset     = 0;     // Preset des eigenen Vereins
  MatchState _matchState     = MatchState::IDLE;
  MatchInfo  _matchInfo;

  // Tagesplan
  bool          _scheduleFetched   = false;
  unsigned long _lastScheduleFetch = 0;   // millis()
  uint32_t      _nextKickoffUTC    = 0;   // wann spielt mein Team heute
  bool          _pollingActive     = false;

  // Tor-Event
  bool          _goalActive    = false;
  bool          _goalMyTeam    = false;
  unsigned long _goalStart     = 0;
  static const  unsigned long GOAL_DURATION = 60000UL;

  // Liga-Suche: Fallback-Index
  int  _leagueIndex    = 0;   // aktueller Index in LeagueChain
  bool _myTeamIsHome   = true;
  uint8_t _lastGoals   = 0;
  uint8_t _homeScore   = 0;
  uint8_t _awayScore   = 0;

  // ---------- FreeRTOS ----------
#ifdef ARDUINO_ARCH_ESP32
  SemaphoreHandle_t _mutex       = nullptr;
  volatile bool     _taskRunning = false;
  volatile bool     _taskDone    = false;

  enum class FetchType : uint8_t {
    SCHEDULE,     // Tagesplan: alle Spiele des Spieltags
    MATCH_LIVE    // Live-Daten: aktuelles Spiel
  };

  struct TaskResult {
    bool      success   = false;
    FetchType type      = FetchType::SCHEDULE;
    MatchInfo match;
    // Für Schedule: nächster Kickoff UTC
    uint32_t  nextKickoffUTC = 0;
  };
  TaskResult _taskResult;

  struct TaskParam {
    char              url[192];
    char              searchKey[32];   // Vereins-Suchstring
    FetchType         type;
    UsermodHSVTicker* self;
  };

  // ---- Fetch-Task ----
  static void fetchTask(void* pv) {
    TaskParam* p = (TaskParam*)pv;
    UsermodHSVTicker* self = p->self;

    char* buf = (char*)malloc(6144);
    bool ok = false;

    if (buf && WiFi.status() == WL_CONNECTED) {
      WiFiClient client;
      HTTPClient http;
      http.setConnectTimeout(8000);
      http.setTimeout(12000);
      http.setReuse(false);
      if (http.begin(client, p->url)) {
        if (http.GET() == HTTP_CODE_OK) {
          WiFiClient* s = http.getStreamPtr();
          int r = 0;
          unsigned long t = millis();
          while (r < 6143 && (millis()-t) < 10000) {
            if (s->available()) buf[r++] = (char)s->read();
            else delay(1);
          }
          buf[r] = '\0';
          ok = (r > 10);
        }
        http.end();
      }
    }

    xSemaphoreTake(self->_mutex, portMAX_DELAY);

    if (ok) {
      // Filter
      StaticJsonDocument<192> filter;
      JsonArray fa = filter.to<JsonArray>();
      JsonObject fo = fa.createNestedObject();
      fo["Team1"]["TeamName"]              = true;
      fo["Team2"]["TeamName"]              = true;
      fo["MatchIsFinished"]                = true;
      fo["MatchDateTime"]                  = true;
      fo["MatchResults"][0]["ResultName"]  = true;
      fo["MatchResults"][0]["PointsTeam1"] = true;
      fo["MatchResults"][0]["PointsTeam2"] = true;

      DynamicJsonDocument* doc = new DynamicJsonDocument(6144);
      if (doc) {
        DeserializationError err = deserializeJson(*doc, buf,
          DeserializationOption::Filter(filter));
        if (!err) {
          String search = String(p->searchKey);
          search.toLowerCase();
          uint32_t nowUTC = (uint32_t)time(nullptr);
          uint32_t bestKickoff = 0;

          for (JsonObject m : doc->as<JsonArray>()) {
            String home = m["Team1"]["TeamName"] | "";
            String away = m["Team2"]["TeamName"] | "";
            String hLow = home; hLow.toLowerCase();
            String aLow = away; aLow.toLowerCase();

            // Kickoff-Zeit parsen (ISO 8601: "2025-05-24T15:30:00")
            String dtStr = m["MatchDateTime"] | "";
            uint32_t kickoffUTC = parseISO8601toUTC(dtStr);

            if (p->type == FetchType::SCHEDULE) {
              // Suche nächstes Spiel des eigenen Teams heute
              bool isMyTeam = (hLow.indexOf(search)>=0 || aLow.indexOf(search)>=0);
              if (isMyTeam && kickoffUTC > nowUTC) {
                if (bestKickoff == 0 || kickoffUTC < bestKickoff)
                  bestKickoff = kickoffUTC;
              }
            } else {
              // Live-Daten: nur eigenes Team
              if (hLow.indexOf(search) < 0 && aLow.indexOf(search) < 0) continue;

              MatchInfo& mi = self->_taskResult.match;
              mi.valid      = true;
              mi.isFinished = m["MatchIsFinished"] | false;
              mi.kickoffUTC = kickoffUTC;
              strncpy(mi.homeTeam, home.c_str(), sizeof(mi.homeTeam)-1);
              strncpy(mi.awayTeam, away.c_str(), sizeof(mi.awayTeam)-1);

              bool found = false;
              for (JsonObject res : m["MatchResults"].as<JsonArray>()) {
                if (String(res["ResultName"]|"") == "Aktuelles Ergebnis") {
                  mi.homeScore = res["PointsTeam1"] | 0;
                  mi.awayScore = res["PointsTeam2"] | 0;
                  mi.isLive    = !mi.isFinished;
                  found = true; break;
                }
              }
              if (!found) {
                for (JsonObject res : m["MatchResults"].as<JsonArray>()) {
                  mi.homeScore = res["PointsTeam1"] | mi.homeScore;
                  mi.awayScore = res["PointsTeam2"] | mi.awayScore;
                }
                // Spiel läuft wenn Kickoff < jetzt und nicht fertig
                mi.isLive = (!mi.isFinished && kickoffUTC > 0 && kickoffUTC < nowUTC);
              }
              self->_taskResult.success = true;
              break;
            }
          }

          if (p->type == FetchType::SCHEDULE) {
            self->_taskResult.nextKickoffUTC = bestKickoff;
            self->_taskResult.success        = true;
          }
        }
        delete doc;
      }
    }

    self->_taskResult.type = p->type;
    self->_taskDone        = true;
    self->_taskRunning     = false;
    xSemaphoreGive(self->_mutex);
    free(buf);
    free(p);
    vTaskDelete(NULL);
  }

  // ISO 8601 → UTC Unix-Timestamp (simpel, ohne DST)
  // Format: "2025-05-24T15:30:00"
  // OpenLigaDB liefert Ortszeit Deutschland → wir subtrahieren UTC-Offset
  // Sommer: UTC+2 → -7200, Winter: UTC+1 → -3600
  // WLED kennt den UTC-Offset via Timezone-Config
  static uint32_t parseISO8601toUTC(const String& s) {
    if (s.length() < 19) return 0;
    int yr  = s.substring(0,4).toInt();
    int mo  = s.substring(5,7).toInt();
    int dy  = s.substring(8,10).toInt();
    int hr  = s.substring(11,13).toInt();
    int mn  = s.substring(14,16).toInt();
    int sc  = s.substring(17,19).toInt();
    // Einfache Unix-Timestamp-Berechnung
    // Tage seit 1970-01-01
    int y = yr - 1970;
    static const int mdays[] = {0,31,59,90,120,151,181,212,243,273,304,334};
    long days = y*365 + (y+1)/4 + mdays[mo-1] + dy - 1;
    if (mo > 2 && (yr%4==0 && (yr%100!=0 || yr%400==0))) days++;
    uint32_t ts = (uint32_t)(days*86400L + hr*3600 + mn*60 + sc);
    // OpenLigaDB: Zeiten sind in lokaler Zeit Deutschland
    // Sommerzeit (März letzter So - Oktober letzter So): UTC+2
    // Winterzeit: UTC+1
    // Vereinfacht: wenn Monat 4-9 → Sommer → -7200, sonst -3600
    int offset = (mo >= 4 && mo <= 9) ? 7200 : 3600;
    return (ts > (uint32_t)offset) ? ts - offset : 0;
  }

  // ---------- Fetch starten ----------
  enum class Phase : uint8_t {
    IDLE,
    FETCH_SCHEDULE,
    FETCH_LIVE,
    WAITING
  } _phase = Phase::IDLE;

  void startFetch(FetchType type, const char* url, const char* searchKey) {
    if (_taskRunning) return;
    TaskParam* p = (TaskParam*)malloc(sizeof(TaskParam));
    if (!p) return;
    strncpy(p->url,       url,       sizeof(p->url)-1);       p->url[sizeof(p->url)-1]       = '\0';
    strncpy(p->searchKey, searchKey, sizeof(p->searchKey)-1); p->searchKey[sizeof(p->searchKey)-1] = '\0';
    p->type    = type;
    p->self    = this;
    _taskDone    = false;
    _taskRunning = true;
    _taskResult  = TaskResult{};
    if (xTaskCreatePinnedToCore(fetchTask,"hsv_fetch",8192,p,1,nullptr,0) != pdPASS) {
      _taskRunning = false;
      free(p);
    }
  }

  // ---------- URL-Builder ----------
  String buildGroupUrl(const char* league) {
    char url[128];
    snprintf(url, sizeof(url),
      "http://api.openligadb.de/getcurrentgroup/%s", league);
    return String(url);
  }

  // Wir nutzen getmatchdata ohne Gruppe → aktueller Spieltag
  // Format: /getmatchdata/{league}/{season}
  String buildMatchdayUrl(const char* league, int season) {
    char url[192];
    snprintf(url, sizeof(url),
      "http://api.openligadb.de/getmatchdata/%s/%d", league, season);
    return String(url);
  }

  // ---------- Suchstring für aktives Preset ----------
  String getSearchKey() {
    // Aus Preset-ID den besten Suchstring finden
    ClubEntry e;
    for (int i = 0; i < ALL_CLUBS_SIZE; i++) {
      memcpy_P(&e, &ALL_CLUBS[i], sizeof(ClubEntry));
      if (e.presetId == _activePreset) return String(e.key);
    }
    if (_activePreset == PRESET_GERMANY) return "deutschland";
    return "";
  }

  // ---------- Schedule verarbeiten ----------
  void processScheduleResult() {
    _scheduleFetched   = true;
    _lastScheduleFetch = millis();

    if (_taskResult.nextKickoffUTC > 0) {
      _nextKickoffUTC = _taskResult.nextKickoffUTC;
      DEBUG_PRINTF("HSVTicker: Naechstes Spiel UTC %u\n", _nextKickoffUTC);
    } else {
      // Kein Spiel in dieser Liga gefunden → nächste Liga versuchen
      _leagueIndex++;
      LeagueChain chain = getLeagueChain(_activePreset);
      if (_leagueIndex < 4 && chain.leagues[_leagueIndex][0] != '\0') {
        _scheduleFetched = false; // nochmal versuchen
      } else {
        _leagueIndex    = 0;
        _nextKickoffUTC = 0;
        DEBUG_PRINTLN(F("HSVTicker: Kein Spiel heute gefunden"));
      }
    }
    _phase = Phase::IDLE;
  }

  // ---------- Live-Daten verarbeiten ----------
  void processLiveResult() {
    if (!_taskResult.success || !_taskResult.match.valid) {
      // Nicht in dieser Liga → nächste Liga
      _leagueIndex++;
      LeagueChain chain = getLeagueChain(_activePreset);
      if (_leagueIndex < 4 && chain.leagues[_leagueIndex][0] != '\0') {
        _phase = Phase::FETCH_LIVE;
      } else {
        _leagueIndex = 0;
        _phase       = Phase::IDLE;
      }
      return;
    }

    _leagueIndex = 0; // Liga gefunden, Reset
    _matchInfo   = _taskResult.match;

    String search = getSearchKey();
    String hLow   = String(_matchInfo.homeTeam); hLow.toLowerCase();
    _myTeamIsHome = (hLow.indexOf(search) >= 0);

    uint8_t totalGoals = _matchInfo.homeScore + _matchInfo.awayScore;

    if (_matchInfo.isFinished) {
      if (_matchState != MatchState::FINISHED) {
        _matchState  = MatchState::FINISHED;
        _pollingActive = false;
        // Farben bleiben (Preset nicht wechseln)
      }
    } else if (_matchInfo.isLive) {
      if (_matchState != MatchState::LIVE && !_goalActive) {
        _matchState = MatchState::LIVE;
        _homeScore  = _matchInfo.homeScore;
        _awayScore  = _matchInfo.awayScore;
        _lastGoals  = totalGoals;
        applyPreset(_activePreset); // Vereinsfarben aktivieren + Sync
      } else if (!_goalActive && totalGoals > _lastGoals) {
        bool homeScored = (_matchInfo.homeScore > _homeScore);
        _homeScore  = _matchInfo.homeScore;
        _awayScore  = _matchInfo.awayScore;
        _lastGoals  = totalGoals;
        bool myGoal = (_myTeamIsHome && homeScored) || (!_myTeamIsHome && !homeScored);
        triggerGoal(myGoal);
      }
    } else {
      _matchState = MatchState::SCHEDULED;
    }
    _phase = Phase::IDLE;
  }

  // ---------- Tor ----------
  void triggerGoal(bool myTeam) {
    _goalActive = true;
    _goalMyTeam = myTeam;
    _goalStart  = millis();
    applyPreset(myTeam ? PRESET_GOAL_MINE : PRESET_GOAL_OPPONENT);
  }

  // ---------- Polling-Timer ----------
  unsigned long _lastPoll = 0;
  static const  unsigned long POLL_INTERVAL = 30000UL;   // 30s live
  static const  unsigned long SCHED_INTERVAL = 86400000UL; // 24h

  bool shouldPoll() {
    if (!_pollingActive) return false;
    return (millis() - _lastPoll > POLL_INTERVAL);
  }

  bool shouldFetchSchedule() {
    if (!_scheduleFetched) return true;
    return (millis() - _lastScheduleFetch > SCHED_INTERVAL);
  }

  // Ist Spielzeit in 5 Minuten oder läuft gerade?
  bool isMatchTime() {
    if (_nextKickoffUTC == 0) return false;
    uint32_t nowUTC = (uint32_t)time(nullptr);
    // Spiel startet in weniger als 5 Min oder läuft (max. 120 Min)
    return (nowUTC >= _nextKickoffUTC - 300 &&
            nowUTC <= _nextKickoffUTC + 7200);
  }
#endif // ARDUINO_ARCH_ESP32

public:

  void setup() override {
#ifdef ARDUINO_ARCH_ESP32
    _mutex = xSemaphoreCreateMutex();
#endif
  }

  void loop() override {
#ifndef ARDUINO_ARCH_ESP32
    return;
#else
    if (!_enabled || !WLED_CONNECTED) return;

    unsigned long now = millis();

    // ── Aktives Preset lesen ──────────────────────────────────
    uint8_t cp = currentPreset; // WLED globale Variable

    // Preset geändert?
    if (cp != _prevPreset) {
      _prevPreset = cp;

      if (cp == PRESET_REFRESH) {
        // Manueller Refresh: Schedule neu laden
        _scheduleFetched = false;
        _leagueIndex     = 0;
        _nextKickoffUTC  = 0;
        _pollingActive   = false;
        _matchState      = MatchState::IDLE;
        // Zurück zum vorherigen aktiven Preset
        if (_activePreset >= PRESET_MIN_ACTIVE && _activePreset != PRESET_REFRESH)
          applyPreset(_activePreset);
        return;
      }

      if (cp < PRESET_MIN_ACTIVE) {
        // Unter 149 → pausieren
        _pollingActive = false;
        return;
      }

      if (cp == PRESET_OFFLINE || cp == PRESET_GOAL_MINE || cp == PRESET_GOAL_OPPONENT) {
        // Spezial-Presets: nicht als aktives Team-Preset setzen
        return;
      }

      // Neues Team-Preset gewählt
      if (cp != _activePreset) {
        _activePreset    = cp;
        _scheduleFetched = false;
        _leagueIndex     = 0;
        _nextKickoffUTC  = 0;
        _pollingActive   = false;
        _matchState      = MatchState::IDLE;
        _goalActive      = false;
        _phase           = Phase::IDLE;
        DEBUG_PRINTF("HSVTicker: Preset %d aktiv\n", cp);
      }
    }

    // Unter 149 → nichts tun
    if (_activePreset < PRESET_MIN_ACTIVE) return;
    if (_activePreset == PRESET_OFFLINE)   return;

    // ── Tor-Event beenden ─────────────────────────────────────
    if (_goalActive && (now - _goalStart >= GOAL_DURATION)) {
      _goalActive = false;
      applyPreset(_activePreset); // zurück zu Vereinsfarben + Sync
    }

    // ── Task-Ergebnis verarbeiten ─────────────────────────────
    if (_taskDone) {
      xSemaphoreTake(_mutex, portMAX_DELAY);
      TaskResult res = _taskResult;
      _taskDone = false;
      xSemaphoreGive(_mutex);

      if (res.type == FetchType::SCHEDULE) {
        // nextKickoffUTC in Ergebnis übernehmen
        _taskResult = res;
        processScheduleResult();
      } else {
        _taskResult = res;
        processLiveResult();
      }
    }

    if (_taskRunning) return;

    // ── Schedule täglich 06:00 UTC laden ─────────────────────
    if (shouldFetchSchedule()) {
      uint32_t nowUTC = (uint32_t)time(nullptr);
      // Nach 06:00 UTC laden (aber nur einmal pro Tag)
      uint32_t secondsToday = nowUTC % 86400;
      bool afterSix = (secondsToday >= 6*3600);
      if (afterSix || !_scheduleFetched) {
        LeagueChain chain = getLeagueChain(_activePreset);
        if (chain.leagues[_leagueIndex][0] != '\0') {
          String url = buildMatchdayUrl(chain.leagues[_leagueIndex], chain.season);
          startFetch(FetchType::SCHEDULE, url.c_str(), getSearchKey().c_str());
          _phase = Phase::WAITING;
          return;
        }
      }
    }

    // ── Polling aktivieren wenn Spielzeit naht ────────────────
    if (!_pollingActive && isMatchTime()) {
      _pollingActive = true;
      _lastPoll      = 0; // sofort pollen
      DEBUG_PRINTLN(F("HSVTicker: Polling gestartet"));
    }

    // ── Live-Polling ──────────────────────────────────────────
    if (shouldPoll() && _phase == Phase::IDLE) {
      _lastPoll = now;
      LeagueChain chain = getLeagueChain(_activePreset);
      if (chain.leagues[_leagueIndex][0] != '\0') {
        String url = buildMatchdayUrl(chain.leagues[_leagueIndex], chain.season);
        startFetch(FetchType::MATCH_LIVE, url.c_str(), getSearchKey().c_str());
        _phase = Phase::WAITING;
      }
    }
#endif
  }

  // ---------- Persistenz ----------
  void addToConfig(JsonObject& root) override {
    JsonObject top = root.createNestedObject(F("HSVTicker"));
    top[F("enabled")] = _enabled;
  }

  bool readFromConfig(JsonObject& root) override {
    JsonObject top = root[F("HSVTicker")];
    if (top.isNull()) return false;
    _enabled = top[F("enabled")] | _enabled;
    return true;
  }

  void appendConfigData() override {
    oappend(SET_F("addInfo('HSVTicker:enabled',1,'Liveticker ein/aus');"));
  }

  // ---------- Info-Panel ----------
  void addToJsonInfo(JsonObject& root) override {
    JsonObject u = root[F("u")];
    if (u.isNull()) u = root.createNestedObject(F("u"));

    if (!_enabled) {
      u.createNestedArray(F("Liveticker")).add("Deaktiviert");
      return;
    }
    if (_activePreset < PRESET_MIN_ACTIVE) {
      u.createNestedArray(F("Liveticker")).add("Pausiert");
      return;
    }

    const char* st = "Bereit";
    switch (_matchState) {
      case MatchState::SCHEDULED: st = "Spiel heute"; break;
      case MatchState::LIVE:      st = "LIVE";        break;
      case MatchState::FINISHED:  st = "Abpfiff";     break;
      default: break;
    }
    if (_goalActive) st = _goalMyTeam ? "TOR! (60s)" : "Gegentor (60s)";

    u.createNestedArray(F("Liveticker")).add(st);

    if (_matchState == MatchState::LIVE || _matchState == MatchState::FINISHED) {
      char s[64];
      snprintf(s, sizeof(s), "%s %u:%u %s",
        _matchInfo.homeTeam, _homeScore, _awayScore, _matchInfo.awayTeam);
      u.createNestedArray(F("Spiel")).add(s);
    }

    if (_nextKickoffUTC > 0 && _matchState == MatchState::SCHEDULED) {
      uint32_t nowUTC  = (uint32_t)time(nullptr);
      int32_t  minLeft = ((int32_t)_nextKickoffUTC - (int32_t)nowUTC) / 60;
      if (minLeft > 0) {
        char s[32];
        snprintf(s, sizeof(s), "in %d Min", minLeft);
        u.createNestedArray(F("Anpfiff")).add(s);
      }
    }
  }

  uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }
};