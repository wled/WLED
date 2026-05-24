#pragma once
/*
 * UsermodHSVTicker - WLED Usermod v2
 * Fussball-Liveticker mit football-data.org API
 * ESP32 DevKit v1 Type C, WLED 0.15.3
 *
 * Rate-Limit-Schutz:
 *  - Mindestens 7s zwischen jedem Request
 *  - 30s Boot-Delay
 *  - Bei 429: 2 Minuten warten
 */

#include "wled.h"

#ifdef ARDUINO_ARCH_ESP32
  #include <HTTPClient.h>
  #include <freertos/semphr.h>
#endif

#define PRESET_REFRESH        149
#define PRESET_OFFLINE        150
#define PRESET_GOAL_MINE      151
#define PRESET_GOAL_OPPONENT  152
#define PRESET_GERMANY        153
#define PRESET_BL_START       154
#define PRESET_BL_END         190
#define PRESET_PL_START       200
#define PRESET_PL_END         223
#define PRESET_LALIGA_START   230
#define PRESET_LALIGA_END     231
#define PRESET_LIGUE1_START   240
#define PRESET_LIGUE1_END     241
#define PRESET_SERIEA_START   250
#define PRESET_SERIEA_END     256

#define FD_BL1   2002
#define FD_BL2   2004
#define FD_BL3   2140
#define FD_PL    2021
#define FD_ELC   2016
#define FD_PD    2014
#define FD_SD    2077
#define FD_FL1   2015
#define FD_FL2   2142
#define FD_SA    2019
#define FD_SB    2121
#define FD_WC    2000
#define FD_EC    2018
#define FD_DFB   2011

static int calcSeason(bool isWM=false, bool isEM=false) {
  time_t n=time(nullptr);
  if (n<100000) return 2025;
  struct tm* t=gmtime(&n);
  int year=t->tm_year+1900, month=t->tm_mon+1;
  if (isWM) return (year<=2026)?2026:year+((4-(year-2026)%4)%4);
  if (isEM) return (year<=2024)?2024:(year<=2028)?2028:year+((4-(year-2028)%4)%4);
  return (month>=8)?year:year-1;
}

struct LeagueChain { int ids[4]; int seasons[4]; };

static LeagueChain getLeagueChain(uint16_t pid) {
  int ls=calcSeason(), wm=calcSeason(true), em=calcSeason(false,true);
  if (pid==PRESET_GERMANY)                                  return {{FD_WC,FD_EC,FD_DFB,0},{wm,em,ls,0}};
  if (pid>=PRESET_BL_START    && pid<=PRESET_BL_END)       return {{FD_BL1,FD_BL2,FD_BL3,0},{ls,ls,ls,0}};
  if (pid>=PRESET_PL_START    && pid<=PRESET_PL_END)       return {{FD_PL,FD_ELC,0,0},{ls,ls,0,0}};
  if (pid>=PRESET_LALIGA_START && pid<=PRESET_LALIGA_END)  return {{FD_PD,FD_SD,0,0},{ls,ls,0,0}};
  if (pid>=PRESET_LIGUE1_START && pid<=PRESET_LIGUE1_END)  return {{FD_FL1,FD_FL2,0,0},{ls,ls,0,0}};
  if (pid>=PRESET_SERIEA_START && pid<=PRESET_SERIEA_END)  return {{FD_SA,FD_SB,0,0},{ls,ls,0,0}};
  return {{0,0,0,0},{0,0,0,0}};
}

struct ClubEntry { char key[22]; uint16_t presetId; };

static const ClubEntry ALL_CLUBS[] PROGMEM = {
  {"hamburg",154},
  {"fuerth",155},{"greuther",155},{"greuter",155},
  {"bayern",156},{"dortmund",157},{"leverkusen",158},{"leipzig",159},
  {"frankfurt",160},{"wolfsburg",161},{"freiburg",162},{"augsburg",163},
  {"mainz",164},{"hoffenheim",165},{"gladbach",166},{"koeln",167},
  {"union berlin",168},{"union",168},{"bochum",169},{"heidenheim",170},
  {"pauli",171},{"kiel",172},{"hertha",173},{"schalke",174},
  {"kaisers",175},{"nuernberg",176},{"darmstadt",177},{"hannover",178},
  {"duesseldorf",179},{"magdeburg",180},{"braunschweig",181},
  {"karlsruhe",182},{"dresden",183},{"paderborn",184},{"elversberg",185},
  {"regensburg",186},{"bielefeld",187},{"saarb",188},{"ulm",189},
  {"werder",190},{"bremen",190},
  {"arsenal",200},{"chelsea",201},{"liverpool",202},
  {"manchester c",203},{"man city",203},
  {"manchester u",204},{"man utd",204},{"man united",204},
  {"tottenham",205},{"hotspur",205},{"spurs",205},
  {"newcastle",206},{"aston villa",207},{"brighton",208},
  {"west ham",209},{"brentford",210},{"fulham",211},{"everton",212},
  {"nottingham",213},{"wolves",214},{"wolverhampton",214},
  {"crystal",215},{"bournemouth",216},{"leicester",217},
  {"ipswich",218},{"southampton",219},{"leeds",220},
  {"sunderland",221},{"burnley",222},{"sheffield",223},
  {"barcelona",230},{"real madrid",231},{"madrid",231},
  {"paris",240},{"psg",240},{"toulouse",241},
  {"milan",250},{"ac milan",250},
  {"internazionale",251},{"inter",251},
  {"juventus",252},
  {"as roma",253},{"roma",253},
  {"lazio",254},{"napoli",255},
  {"torino",256},{"toro",256},
};
static const int ALL_CLUBS_SIZE=sizeof(ALL_CLUBS)/sizeof(ClubEntry);

enum class MatchState : uint8_t { IDLE=0, SCHEDULED, LIVE, FINISHED };

struct MatchInfo {
  bool valid=false,isLive=false,isFinished=false;
  uint8_t homeScore=0,awayScore=0;
  uint32_t kickoffUTC=0;
  char homeTeam[48]="",awayTeam[48]="";
};

class UsermodHSVTicker : public Usermod {
private:
  bool _enabled=true;
  char _token[48]="";
  bool _tokenLoaded=false;

  uint16_t   _activePreset=0,_prevPreset=0;
  MatchState _matchState=MatchState::IDLE;
  MatchInfo  _matchInfo;
  bool       _myTeamIsHome=true;
  uint8_t    _lastGoals=0,_homeScore=0,_awayScore=0;
  int        _leagueIndex=0,_foundLeagueId=0,_currentMatchday=-1;
  bool       _scheduleFetched=false;
  unsigned long _lastScheduleFetch=0;
  uint32_t   _nextKickoffUTC=0;
  bool       _pollingActive=false;
  bool       _goalActive=false,_goalMyTeam=false;
  unsigned long _goalStart=0,_lastPoll=0;
  int        _lastHttpCode=0;
  unsigned long _taskStarted=0;

  // Rate-Limit-Schutz: letzter Request-Zeitpunkt
  unsigned long _lastRequestTime=0;

  static const unsigned long GOAL_DURATION   = 60000UL;
  static const unsigned long POLL_INTERVAL   = 30000UL;
  static const unsigned long SCHED_INTERVAL  = 86400000UL;
  static const unsigned long BOOT_DELAY      = 30000UL;
  // Mindestabstand zwischen Requests (7s = 8 Req/Min, sicher unter 10)
  static const unsigned long MIN_REQUEST_GAP = 7000UL;
  // Wartezeit bei 429
  static const unsigned long RATE_LIMIT_WAIT = 120000UL;

#ifdef ARDUINO_ARCH_ESP32
  SemaphoreHandle_t _mutex=nullptr;
  volatile bool _taskRunning=false,_taskDone=false;

  enum class FetchType : uint8_t { SCHEDULE, LIVE };

  struct TaskResult {
    bool success=false,teamFound=false,matchIsLiveNow=false;
    FetchType type=FetchType::SCHEDULE;
    MatchInfo match;
    uint32_t nextKickoffUTC=0;
    int currentMatchday=-1;
    int httpCode=0;
  };
  TaskResult _taskResult;

  struct TaskParam {
    char url[256],token[48],searchKey[32];
    FetchType type;
    UsermodHSVTicker* self;
  };

  static uint32_t parseUTC(const char* s) {
    if (!s||strlen(s)<19) return 0;
    int yr=0,mo=0,dy=0,hr=0,mn=0,sc=0;
    sscanf(s,"%d-%d-%dT%d:%d:%d",&yr,&mo,&dy,&hr,&mn,&sc);
    if (yr<2020) return 0;
    static const int md[]={0,31,59,90,120,151,181,212,243,273,304,334};
    int y=yr-1970;
    long days=(long)y*365+(y+1)/4+md[mo-1]+dy-1;
    if (mo>2&&(yr%4==0&&(yr%100!=0||yr%400==0))) days++;
    return (uint32_t)(days*86400L+hr*3600+mn*60+sc);
  }

  static int extractMatchday(const char* buf) {
    const char* p=buf;
    while ((p=strstr(p,"currentMatchday"))!=nullptr) {
      const char* q=p+15;
      while (*q&&(*q=='"'||*q==':')) q++;
      if (*q>='0'&&*q<='9') {
        int val=atoi(q);
        if (val>0&&val<=50) return val;
      }
      p++;
    }
    return -1;
  }

  static void fetchTask(void* pv) {
    TaskParam* p=(TaskParam*)pv;
    UsermodHSVTicker* self=p->self;
    char* buf=(char*)malloc(8192);
    bool ok=false;
    int httpCode=0;

    if (buf&&WiFi.status()==WL_CONNECTED&&p->token[0]) {
      WiFiClient client;
      HTTPClient http;
      http.setConnectTimeout(8000);
      http.setTimeout(12000);
      http.setReuse(false);
      if (http.begin(client,p->url)) {
        http.addHeader("X-Auth-Token",p->token);
        http.addHeader("Accept","application/json");
        httpCode=http.GET();
        if (httpCode==HTTP_CODE_OK) {
          String body=http.getString();
          size_t len=body.length();
          if (len>0&&len<8191) {
            memcpy(buf,body.c_str(),len+1);
            ok=(len>10);
          }
        }
        http.end();
      }
    }

    xSemaphoreTake(self->_mutex,portMAX_DELAY);
    self->_taskResult.httpCode=httpCode;

    if (ok) {
      self->_taskResult.currentMatchday=extractMatchday(buf);

      StaticJsonDocument<200> filter;
      filter["matches"][0]["utcDate"]                   =true;
      filter["matches"][0]["status"]                    =true;
      filter["matches"][0]["homeTeam"]["name"]          =true;
      filter["matches"][0]["awayTeam"]["name"]          =true;
      filter["matches"][0]["score"]["fullTime"]["home"] =true;
      filter["matches"][0]["score"]["fullTime"]["away"] =true;

      DynamicJsonDocument* doc=new DynamicJsonDocument(8192);
      if (doc) {
        if (!deserializeJson(*doc,buf,DeserializationOption::Filter(filter))) {
          String search=String(p->searchKey); search.toLowerCase();
          uint32_t nowUTC=(uint32_t)time(nullptr);
          uint32_t bestKickoff=0;
          bool liveNow=false;

          for (JsonObject m : (*doc)["matches"].as<JsonArray>()) {
            String home=m["homeTeam"]["name"]|"";
            String away=m["awayTeam"]["name"]|"";
            String status=m["status"]|"";
            const char* utcDate=m["utcDate"]|"";
            uint32_t kickoff=parseUTC(utcDate);
            String hLow=home; hLow.toLowerCase();
            String aLow=away; aLow.toLowerCase();
            bool myTeam=(hLow.indexOf(search)>=0||aLow.indexOf(search)>=0);

            if (p->type==FetchType::SCHEDULE) {
              if (!myTeam) continue;
              if (status=="IN_PLAY"||status=="PAUSED") {
                bestKickoff=(kickoff>0)?kickoff:nowUTC-3600;
                liveNow=true; break;
              }
              if (kickoff>nowUTC&&kickoff<nowUTC+86400) {
                if (!bestKickoff||kickoff<bestKickoff) bestKickoff=kickoff;
              }
            } else {
              if (!myTeam) continue;
              MatchInfo& mi=self->_taskResult.match;
              mi.valid=true; mi.kickoffUTC=kickoff;
              strncpy(mi.homeTeam,home.c_str(),sizeof(mi.homeTeam)-1);
              strncpy(mi.awayTeam,away.c_str(),sizeof(mi.awayTeam)-1);
              mi.isLive=(status=="IN_PLAY"||status=="PAUSED");
              mi.isFinished=(status=="FINISHED");
              int sh=m["score"]["fullTime"]["home"]|-1;
              int sa=m["score"]["fullTime"]["away"]|-1;
              if (sh>=0) mi.homeScore=(uint8_t)sh;
              if (sa>=0) mi.awayScore=(uint8_t)sa;
              self->_taskResult.teamFound=true;
              self->_taskResult.success=true;
              break;
            }
          }
          if (p->type==FetchType::SCHEDULE) {
            self->_taskResult.nextKickoffUTC=bestKickoff;
            self->_taskResult.teamFound=(bestKickoff>0);
            self->_taskResult.matchIsLiveNow=liveNow;
            self->_taskResult.success=true;
          }
        }
        delete doc;
      }
    }

    self->_taskResult.type=p->type;
    self->_taskDone=true;
    self->_taskRunning=false;
    xSemaphoreGive(self->_mutex);
    free(buf); free(p);
    vTaskDelete(NULL);
  }

  enum class Phase : uint8_t { IDLE, WAITING } _phase=Phase::IDLE;

  // Prueft ob ein neuer Request erlaubt ist (Rate-Limit-Schutz)
  bool canRequest() {
    unsigned long now=millis();
    // Bei 429: 2 Minuten warten
    if (_lastHttpCode==429 && (now-_lastRequestTime)<RATE_LIMIT_WAIT) return false;
    // Sonst: mindestens 7s zwischen Requests
    if ((now-_lastRequestTime)<MIN_REQUEST_GAP) return false;
    return true;
  }

  void startFetch(FetchType type, int compId) {
    if (_taskRunning||!_token[0]) return;
    if (!canRequest()) return;  // Rate-Limit-Schutz

    TaskParam* p=(TaskParam*)malloc(sizeof(TaskParam));
    if (!p) return;

    LeagueChain chain=getLeagueChain(_activePreset);
    int season=2025;
    for (int i=0;i<4;i++) if (chain.ids[i]==compId){season=chain.seasons[i];break;}

    char url[256];
    if (type==FetchType::LIVE&&_currentMatchday>0) {
      snprintf(url,sizeof(url),
        "http://api.football-data.org/v4/competitions/%d/matches?season=%d&matchday=%d",
        compId,season,_currentMatchday);
    } else if (type==FetchType::LIVE) {
      snprintf(url,sizeof(url),
        "http://api.football-data.org/v4/competitions/%d/matches?season=%d&status=IN_PLAY,PAUSED,FINISHED",
        compId,season);
    } else {
      snprintf(url,sizeof(url),
        "http://api.football-data.org/v4/competitions/%d/matches?season=%d&status=SCHEDULED,IN_PLAY,PAUSED",
        compId,season);
    }

    strncpy(p->url,url,sizeof(p->url)-1);       p->url[sizeof(p->url)-1]='\0';
    strncpy(p->token,_token,sizeof(p->token)-1); p->token[sizeof(p->token)-1]='\0';
    String sk=getSearchKey();
    strncpy(p->searchKey,sk.c_str(),sizeof(p->searchKey)-1);
    p->searchKey[sizeof(p->searchKey)-1]='\0';
    p->type=type; p->self=this;
    _taskDone=false; _taskRunning=true; _taskResult=TaskResult{};
    _taskStarted=millis();
    _lastRequestTime=millis(); // Zeitstempel setzen

    if (xTaskCreatePinnedToCore(fetchTask,"hsv_fetch",12288,p,1,nullptr,0)!=pdPASS) {
      _taskRunning=false; free(p);
    }
  }

  void processScheduleResult(TaskResult& res) {
    _scheduleFetched=true;
    _lastScheduleFetch=millis();
    _lastHttpCode=res.httpCode;
    if (res.currentMatchday>0) _currentMatchday=res.currentMatchday;

    if (res.teamFound&&res.nextKickoffUTC>0) {
      _nextKickoffUTC=res.nextKickoffUTC;
      _foundLeagueId=getCurrentLeagueId();
      _leagueIndex=0;
      if (res.matchIsLiveNow) { _pollingActive=true; _lastPoll=0; }
    } else {
      _leagueIndex++;
      LeagueChain chain=getLeagueChain(_activePreset);
      if (_leagueIndex<4&&chain.ids[_leagueIndex]!=0) {
        _scheduleFetched=false;
        _lastScheduleFetch=millis()-(SCHED_INTERVAL-6000);
      } else {
        _leagueIndex=0; _nextKickoffUTC=0; _foundLeagueId=0;
      }
    }
    _phase=Phase::IDLE;
  }

  void processLiveResult(TaskResult& res) {
    _lastHttpCode=res.httpCode;
    if (res.currentMatchday>0) _currentMatchday=res.currentMatchday;
    if (!res.teamFound||!res.match.valid) {
      _leagueIndex++;
      LeagueChain chain=getLeagueChain(_activePreset);
      if (_leagueIndex>=4||chain.ids[_leagueIndex]==0) _leagueIndex=0;
      _lastPoll=0; _phase=Phase::IDLE; return;
    }
    _leagueIndex=0; _matchInfo=res.match;
    String search=getSearchKey(); search.toLowerCase();
    String hLow=String(_matchInfo.homeTeam); hLow.toLowerCase();
    _myTeamIsHome=(hLow.indexOf(search)>=0);
    uint8_t totalGoals=_matchInfo.homeScore+_matchInfo.awayScore;

    if (_matchInfo.isFinished) {
      if (_matchState!=MatchState::FINISHED) {
        _matchState=MatchState::FINISHED; _pollingActive=false;
      }
    } else if (_matchInfo.isLive) {
      if (_matchState!=MatchState::LIVE&&!_goalActive) {
        _matchState=MatchState::LIVE;
        _homeScore=_matchInfo.homeScore; _awayScore=_matchInfo.awayScore;
        _lastGoals=totalGoals;
        applyPreset(_activePreset);
      } else if (!_goalActive&&totalGoals>_lastGoals) {
        bool homeScored=(_matchInfo.homeScore>_homeScore);
        _homeScore=_matchInfo.homeScore; _awayScore=_matchInfo.awayScore;
        _lastGoals=totalGoals;
        bool myGoal=(_myTeamIsHome&&homeScored)||(!_myTeamIsHome&&!homeScored);
        _goalActive=true; _goalMyTeam=myGoal; _goalStart=millis();
        applyPreset(myGoal?PRESET_GOAL_MINE:PRESET_GOAL_OPPONENT);
      }
    } else { _matchState=MatchState::SCHEDULED; }
    _phase=Phase::IDLE;
  }

  String getSearchKey() {
    ClubEntry e;
    for (int i=0;i<ALL_CLUBS_SIZE;i++) {
      memcpy_P(&e,&ALL_CLUBS[i],sizeof(ClubEntry));
      if (e.presetId==_activePreset) return String(e.key);
    }
    return (_activePreset==PRESET_GERMANY)?"germany":"";
  }

  int getCurrentLeagueId() {
    LeagueChain c=getLeagueChain(_activePreset);
    return (_leagueIndex<4)?c.ids[_leagueIndex]:0;
  }

  bool isMatchTime() {
    if (!_nextKickoffUTC) return false;
    uint32_t n=(uint32_t)time(nullptr);
    return (n>=_nextKickoffUTC-300&&n<=_nextKickoffUTC+9000);
  }
#endif

public:
  void setup() override {
#ifdef ARDUINO_ARCH_ESP32
    _mutex=xSemaphoreCreateMutex();
#endif
  }

  void loop() override {
#ifndef ARDUINO_ARCH_ESP32
    return;
#else
    if (millis()<BOOT_DELAY) return;

    // Token-Fix: cfg.json direkt lesen
    if (!_tokenLoaded) {
      if (WLED_FS.exists("/cfg.json")) {
        File f=WLED_FS.open("/cfg.json","r");
        if (f) {
          DynamicJsonDocument* doc=new DynamicJsonDocument(4096);
          if (doc) {
            if (!deserializeJson(*doc,f)) {
              JsonObject ht=(*doc)["um"]["HSVTicker"];
              if (!ht.isNull()) {
                const char* t=ht["token"]|"";
                if (t&&strlen(t)>0) strlcpy(_token,t,sizeof(_token));
                _enabled=ht["enabled"]|_enabled;
              }
            }
            delete doc;
          }
          f.close();
        }
      }
      _tokenLoaded=true;
    }

    if (!_enabled||!WLED_CONNECTED||!_token[0]) return;

    unsigned long now=millis();
    uint16_t cp=currentPreset;

    if (cp!=_prevPreset) {
      _prevPreset=cp;
      if (cp==PRESET_REFRESH) {
        _scheduleFetched=false; _leagueIndex=0; _nextKickoffUTC=0;
        _pollingActive=false; _matchState=MatchState::IDLE;
        _goalActive=false; _currentMatchday=-1; _foundLeagueId=0;
        _lastHttpCode=0; // Reset Rate-Limit-Status
        if (_activePreset>=PRESET_REFRESH&&_activePreset!=PRESET_REFRESH)
          applyPreset(_activePreset);
        return;
      }
      if (cp==PRESET_GOAL_MINE||cp==PRESET_GOAL_OPPONENT||cp==PRESET_OFFLINE) return;
      if (cp<PRESET_REFRESH) { _pollingActive=false; return; }
      if (cp!=_activePreset) {
        _activePreset=cp; _scheduleFetched=false; _leagueIndex=0;
        _nextKickoffUTC=0; _foundLeagueId=0; _pollingActive=false;
        _matchState=MatchState::IDLE; _goalActive=false;
        _phase=Phase::IDLE; _currentMatchday=-1;
        _lastHttpCode=0; // Reset bei Preset-Wechsel
      }
    }
    if (_activePreset<PRESET_REFRESH||_activePreset==PRESET_OFFLINE) return;

    // Tor-Event beenden
    if (_goalActive&&(now-_goalStart>=GOAL_DURATION)) {
      _goalActive=false; applyPreset(_activePreset);
    }

    // Task-Timeout nach 20s
    if (_taskRunning&&(now-_taskStarted>20000)) {
      _taskRunning=false; _taskDone=false; _phase=Phase::IDLE;
    }

    // Task-Ergebnis verarbeiten
    if (_taskDone) {
      xSemaphoreTake(_mutex,portMAX_DELAY);
      TaskResult res=_taskResult; _taskDone=false;
      xSemaphoreGive(_mutex);
      if (res.type==FetchType::SCHEDULE) processScheduleResult(res);
      else processLiveResult(res);
    }
    if (_taskRunning||_phase==Phase::WAITING) return;

    // Schedule laden (Rate-Limit-Schutz beachten)
    if (!_scheduleFetched||(now-_lastScheduleFetch>SCHED_INTERVAL)) {
      int lid=getCurrentLeagueId();
      if (lid&&canRequest()) {
        startFetch(FetchType::SCHEDULE,lid);
        _phase=Phase::WAITING;
        return;
      }
    }

    // Polling aktivieren wenn Spielzeit naht
    if (!_pollingActive&&_scheduleFetched&&isMatchTime()) {
      _pollingActive=true; _lastPoll=0;
    }

    // Live-Polling (Rate-Limit-Schutz beachten)
    if (_pollingActive&&(now-_lastPoll>POLL_INTERVAL)&&canRequest()) {
      _lastPoll=now;
      int lid=_foundLeagueId?_foundLeagueId:getCurrentLeagueId();
      if (lid) { startFetch(FetchType::LIVE,lid); _phase=Phase::WAITING; }
    }
#endif
  }

  void addToConfig(JsonObject& root) override {
    JsonObject top=root.createNestedObject(F("HSVTicker"));
    top[F("enabled")]=_enabled; top[F("token")]=_token;
  }

  bool readFromConfig(JsonObject& root) override {
    JsonObject top=root[F("HSVTicker")];
    if (top.isNull()) return false;
    _enabled=top[F("enabled")]|_enabled;
    const char* t=top[F("token")]|"";
    if (t&&strlen(t)>0) strlcpy(_token,t,sizeof(_token));
    return true;
  }

  void appendConfigData() override {
    oappend(SET_F("addInfo('HSVTicker:enabled',1,'Liveticker ein/aus');"));
    oappend(SET_F("addInfo('HSVTicker:token',1,'football-data.org API Token');"));
  }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject u=root[F("u")];
    if (u.isNull()) u=root.createNestedObject(F("u"));

    if (!_enabled)                    { u.createNestedArray(F("Ticker")).add("Deaktiviert"); return; }
    if (!_token[0])                   { u.createNestedArray(F("Ticker")).add("Kein Token!"); return; }
    if (_activePreset<PRESET_REFRESH) { u.createNestedArray(F("Ticker")).add("Pausiert"); return; }

    const char* st="Bereit";
    switch(_matchState) {
      case MatchState::SCHEDULED: st="Spiel heute"; break;
      case MatchState::LIVE:      st="LIVE";        break;
      case MatchState::FINISHED:  st="Abpfiff";     break;
      default: break;
    }
    if (_goalActive) st=_goalMyTeam?"TOR! (60s)":"Gegentor (60s)";
    u.createNestedArray(F("Ticker")).add(st);

    if (_matchState==MatchState::LIVE||_matchState==MatchState::FINISHED) {
      char s[64];
      snprintf(s,sizeof(s),"%s %u:%u %s",
        _matchInfo.homeTeam,_homeScore,_awayScore,_matchInfo.awayTeam);
      u.createNestedArray(F("Spiel")).add(s);
    }
    if (_nextKickoffUTC&&_matchState==MatchState::SCHEDULED) {
      int32_t ml=((int32_t)_nextKickoffUTC-(int32_t)time(nullptr))/60;
      if (ml>0&&ml<1440) {
        char s[32]; snprintf(s,sizeof(s),"in %d Min",ml);
        u.createNestedArray(F("Anpfiff")).add(s);
      }
    }
    // Debug-Info
    unsigned long gap=(millis()-_lastRequestTime)/1000;
    char dbg[56];
    snprintf(dbg,sizeof(dbg),"P%d MD%d HTTP%d %s gap%lus",
      _activePreset,_currentMatchday,_lastHttpCode,
      _pollingActive?"Poll":"Warte",gap);
    u.createNestedArray(F("Debug")).add(dbg);
  }

  uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }
};