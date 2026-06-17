#pragma once
/*
 * UsermodHSVTicker - WLED Usermod v2 - Neuschrieb
 * Einfache Architektur: synchroner Request alle 30s im loop()
 *
 * Gelernte Lektionen:
 * - fd.org: Raw TCP Port 443, Chunked Transfer-Encoding, Accept-Encoding:identity
 * - OpenLigaDB: HTTPS mit WiFiClientSecure, kein Token
 * - Buffer: dynamisch basierend auf Heap
 * - Kein FreeRTOS Task - kein Race Condition Problem
 */

#include "wled.h"
#ifdef ARDUINO_ARCH_ESP32
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// ── Presets ──────────────────────────────────────────────────────────────────
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

// ── Liga IDs (fd.org) ────────────────────────────────────────────────────────
#define FD_WC    2000
#define FD_EC    2018
#define FD_PL    2021
#define FD_ELC   2016
#define FD_PD    2014
#define FD_SA    2019
#define FD_FL1   2015

// ── OpenLigaDB Kürzel (negative IDs) ─────────────────────────────────────────
#define OL_BL1   -1
#define OL_BL2   -2
#define OL_BL3   -3
#define OL_BLREL -4
#define OL_DFB   -5

// ── Timing ───────────────────────────────────────────────────────────────────
#define POLL_LIVE_MS      30000UL   // Live-Polling Intervall
#define POLL_SCHED_MS     300000UL  // Schedule-Check (5 Min)
#define GOAL_DURATION_MS  60000UL   // Tor-Preset Dauer
#define BOOT_DELAY_MS     10000UL   // Warten nach Boot

// ── Club-Datenbank ────────────────────────────────────────────────────────────
struct ClubEntry { char key[32]; uint16_t presetId; };
static const ClubEntry ALL_CLUBS[] PROGMEM = {
  {"hamburg",154},
  {"fürth|fuerth",155},{"greuther",155},
  {"bayern",156},{"dortmund",157},{"leverkusen",158},{"leipzig",159},
  {"frankfurt",160},{"wolfsburg",161},{"freiburg",162},{"augsburg",163},
  {"mainz",164},{"hoffenheim",165},{"gladbach",166},{"köln|koeln",167},
  {"union berlin",168},{"bochum",169},{"heidenheim",170},
  {"pauli",171},{"kiel",172},{"hertha",173},{"schalke",174},
  {"kaisers",175},{"nürnberg|nuernberg",176},{"darmstadt",177},{"hannover",178},
  {"düsseldorf|duesseldorf",179},{"magdeburg",180},{"braunschweig",181},
  {"karlsruhe",182},{"dresden",183},{"paderborn",184},{"elversberg",185},
  {"regensburg",186},{"bielefeld",187},{"saarb",188},{"ulm",189},
  {"werder",190},{"bremen",190},
  {"arsenal",200},{"chelsea",201},{"liverpool",202},
  {"manchester c",203},{"man city",203},
  {"manchester u",204},{"man utd",204},
  {"tottenham",205},{"newcastle",206},{"aston villa",207},
  {"brighton",208},{"west ham",209},{"brentford",210},
  {"fulham",211},{"everton",212},{"nottingham",213},
  {"wolves",214},{"crystal",215},{"bournemouth",216},
  {"leicester",217},{"ipswich",218},{"southampton",219},
  {"leeds",220},{"sunderland",221},{"burnley",222},{"sheffield",223},
  {"barcelona",230},{"real madrid",231},
  {"paris",240},{"psg",240},
  {"milan",250},{"inter",251},{"juventus",252},
  {"roma",253},{"lazio",254},{"napoli",255},{"torino",256},
};
static const int ALL_CLUBS_SIZE = sizeof(ALL_CLUBS)/sizeof(ClubEntry);

// ── Liga-Kette pro Preset ─────────────────────────────────────────────────────
struct LeagueChain { int ids[4]; };
static LeagueChain getLeagueChain(uint16_t pid) {
  if (pid==PRESET_GERMANY)                                   return {{FD_WC,FD_EC,0,0}};
  if (pid>=PRESET_BL_START    && pid<=PRESET_BL_END)        return {{OL_BL1,OL_BL2,OL_BL3,OL_DFB}};
  if (pid>=PRESET_PL_START    && pid<=PRESET_PL_END)        return {{FD_PL,FD_ELC,0,0}};
  if (pid>=PRESET_LALIGA_START && pid<=PRESET_LALIGA_END)   return {{FD_PD,0,0,0}};
  if (pid>=PRESET_LIGUE1_START && pid<=PRESET_LIGUE1_END)   return {{FD_FL1,0,0,0}};
  if (pid>=PRESET_SERIEA_START && pid<=PRESET_SERIEA_END)   return {{FD_SA,0,0,0}};
  return {{0,0,0,0}};
}

// ── Team-Suche mit | Separator ────────────────────────────────────────────────
static bool teamMatches(const String& name, const char* key) {
  String n=name; n.toLowerCase();
  String k=String(key); k.toLowerCase();
  int start=0;
  while (true) {
    int sep=k.indexOf('|',start);
    String part=(sep<0)?k.substring(start):k.substring(start,sep);
    part.trim();
    if (part.length()>0 && n.indexOf(part)>=0) return true;
    if (sep<0) break;
    start=sep+1;
  }
  return false;
}

// ── Suchbegriff für Preset ───────────────────────────────────────────────────
static String getSearchKey(uint16_t pid) {
  if (pid==PRESET_GERMANY) return "germany|deutsch";
  ClubEntry e;
  for (int i=0;i<ALL_CLUBS_SIZE;i++) {
    memcpy_P(&e,&ALL_CLUBS[i],sizeof(ClubEntry));
    if (e.presetId==pid) return String(e.key);
  }
  return "";
}

// ── UTC-Parser ────────────────────────────────────────────────────────────────
static uint32_t parseUTC(const char* s) {
  if (!s||strlen(s)<19) return 0;
  int yr,mo,dy,hr,mn,sc2;
  sscanf(s,"%d-%d-%dT%d:%d:%d",&yr,&mo,&dy,&hr,&mn,&sc2);
  if (yr<2020) return 0;
  static const int md[]={0,31,59,90,120,151,181,212,243,273,304,334};
  int y=yr-1970;
  long days=(long)y*365+(y+1)/4+md[mo-1]+dy-1;
  if (mo>2&&(yr%4==0&&(yr%100!=0||yr%400==0))) days++;
  return (uint32_t)(days*86400L+hr*3600+mn*60+sc2);
}

// ── Log ───────────────────────────────────────────────────────────────────────
static void writeLog(const char* msg) {
  File f=WLED_FS.open("/hsv_log.txt","r");
  size_t sz=f?f.size():0; if(f) f.close();
  f=WLED_FS.open("/hsv_log.txt",(sz>3800)?"w":"a");
  if (!f) return;
  char line[128];
  snprintf(line,sizeof(line),"[%lu] %s\n",(unsigned long)time(nullptr),msg);
  f.print(line); f.close();
}

// ── Raw TCP Request zu fd.org ─────────────────────────────────────────────────
static int fdRequest(const char* path, const char* token, char* buf, size_t bufSize) {
  WiFiClientSecure sc;
  sc.setInsecure();
  sc.setTimeout(20);
  if (!sc.connect("api.football-data.org",443)) return -1;

  // Request senden
  sc.print("GET "); sc.print(path); sc.println(" HTTP/1.1");
  sc.println("Host: api.football-data.org");
  sc.print("X-Auth-Token: "); sc.println(token);
  sc.println("Accept: application/json");
  sc.println("Accept-Encoding: identity");
  sc.println("Connection: close");
  sc.println();

  // Header lesen
  unsigned long t=millis();
  char hbuf[1024]; int hlen=0;
  bool headerDone=false, chunked=false;
  int httpCode=0;

  while ((millis()-t)<20000&&(sc.connected()||sc.available())) {
    if (!sc.available()) { delay(1); continue; }
    char c=(char)sc.read();
    if (!headerDone) {
      if (hlen<1022) hbuf[hlen++]=c;
      hbuf[hlen]=0;
      if (hlen>=4&&hbuf[hlen-4]=='\r'&&hbuf[hlen-3]=='\n'&&
          hbuf[hlen-2]=='\r'&&hbuf[hlen-1]=='\n') {
        headerDone=true;
        const char* hs=strstr(hbuf,"HTTP/1.");
        if (hs) httpCode=atoi(hs+9);
        chunked=(strstr(hbuf,"Transfer-Encoding: chunked")!=nullptr||
                 strstr(hbuf,"transfer-encoding: chunked")!=nullptr);
      }
    } else break; // Body-Lesen unten
  }

  if (!headerDone||httpCode!=200) { sc.stop(); return httpCode?httpCode:-2; }

  // Body lesen
  int r=0;
  t=millis();
  if (chunked) {
    while ((millis()-t)<15000&&(sc.connected()||sc.available())) {
      // Chunk-Size lesen
      char cline[16]; int cl=0;
      unsigned long ct=millis();
      while ((millis()-ct)<2000) {
        if (sc.available()) {
          char cc=(char)sc.read();
          if (cl<15) cline[cl++]=cc;
          if (cl>=2&&cline[cl-2]=='\r'&&cline[cl-1]=='\n') break;
        } else delay(1);
      }
      cline[cl]=0;
      int chunkSize=(int)strtol(cline,nullptr,16);
      if (chunkSize==0) break;
      int got=0;
      ct=millis();
      while (got<chunkSize&&(millis()-ct)<8000) {
        if (sc.available()) {
          char cd=(char)sc.read();
          if (r<(int)bufSize-1) buf[r++]=cd;
          got++;
        } else delay(1);
      }
      buf[r]=0;
      // trailing \r\n
      ct=millis();
      int crlf=0;
      while (crlf<2&&(millis()-ct)<1000) {
        if (sc.available()) { char cd=(char)sc.read(); if(cd=='\r'||cd=='\n') crlf++; else break; }
        else delay(1);
      }
    }
  } else {
    while ((millis()-t)<15000&&(sc.connected()||sc.available())) {
      if (sc.available()) { char c=(char)sc.read(); if(r<(int)bufSize-1) buf[r++]=c; }
      else delay(1);
    }
  }
  buf[r]=0;
  sc.stop();
  return (r>10)?200:-3;
}

// ── OpenLigaDB Request ────────────────────────────────────────────────────────
static int olRequest(const char* league, char* buf, size_t bufSize) {
  WiFiClientSecure sc;
  sc.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(12000);
  char url[128];
  snprintf(url,sizeof(url),"https://api.openligadb.de/getmatchdata/%s",league);
  if (!http.begin(sc,url)) return -1;
  http.addHeader("Accept","application/json");
  int code=http.GET();
  if (code==200) {
    String body=http.getString();
    size_t len=body.length();
    if (len>0&&len<bufSize-1) { memcpy(buf,body.c_str(),len+1); }
    else if (len>0) { memcpy(buf,body.c_str(),bufSize-1); buf[bufSize-1]=0; }
  }
  http.end();
  return code;
}

// ── Spiel-Ergebnis ────────────────────────────────────────────────────────────
struct MatchResult {
  bool found=false, isLive=false, isFinished=false, isPaused=false;
  uint8_t homeScore=0, awayScore=0;
  uint32_t kickoffUTC=0;
  char homeTeam[48]="", awayTeam[48]="";
};

// ── fd.org JSON parsen ────────────────────────────────────────────────────────
static MatchResult parseFD(const char* buf, const char* searchKey, bool scheduleMode) {
  MatchResult res;
  StaticJsonDocument<512> filter;
  filter["matches"][0]["utcDate"]=true;
  filter["matches"][0]["status"]=true;
  filter["matches"][0]["homeTeam"]["name"]=true;
  filter["matches"][0]["awayTeam"]["name"]=true;
  filter["matches"][0]["score"]["fullTime"]["home"]=true;
  filter["matches"][0]["score"]["fullTime"]["away"]=true;

  size_t docSize=min((size_t)24576,ESP.getMaxAllocHeap()/2);
  DynamicJsonDocument* doc=new DynamicJsonDocument(docSize);
  if (!doc) return res;

  DeserializationError err=deserializeJson(*doc,buf,DeserializationOption::Filter(filter));
  if (!err) {
    uint32_t now=(uint32_t)time(nullptr);
    for (JsonObject m : (*doc)["matches"].as<JsonArray>()) {
      String home=m["homeTeam"]["name"]|"";
      String away=m["awayTeam"]["name"]|"";
      String status=m["status"]|"";
      uint32_t kickoff=parseUTC(m["utcDate"]|"");
      if (!teamMatches(home,searchKey)&&!teamMatches(away,searchKey)) continue;
      if (scheduleMode) {
        if (status=="IN_PLAY"||status=="PAUSED") {
          res.found=true; res.kickoffUTC=kickoff?kickoff:now-3600; res.isLive=true; break;
        }
        if (status=="TIMED"||status=="SCHEDULED") {
          if (kickoff>now&&kickoff<now+86400) { res.found=true; res.kickoffUTC=kickoff; break; }
        }
        if (status=="FINISHED"&&kickoff>now-7200) {
          // Spiel heute beendet - als gefunden markieren damit Polling stoppt
          res.found=true; res.kickoffUTC=kickoff; break;
        }
      } else {
        res.found=true;
        res.isLive=(status=="IN_PLAY");
        res.isPaused=(status=="PAUSED");
        res.isFinished=(status=="FINISHED");
        res.kickoffUTC=kickoff;
        strncpy(res.homeTeam,home.c_str(),47);
        strncpy(res.awayTeam,away.c_str(),47);
        int sh=m["score"]["fullTime"]["home"]|-1;
        int sa=m["score"]["fullTime"]["away"]|-1;
        if (sh>=0) res.homeScore=(uint8_t)sh;
        if (sa>=0) res.awayScore=(uint8_t)sa;
        break;
      }
    }
  }
  delete doc;
  return res;
}

// ── OpenLigaDB JSON parsen ────────────────────────────────────────────────────
static MatchResult parseOL(const char* buf, const char* searchKey, bool scheduleMode) {
  MatchResult res;
  size_t docSize=min((size_t)16384,ESP.getMaxAllocHeap()/2);
  DynamicJsonDocument* doc=new DynamicJsonDocument(docSize);
  if (!doc) return res;

  DeserializationError err=deserializeJson(*doc,buf);
  if (!err) {
    uint32_t now=(uint32_t)time(nullptr);
    for (JsonObject m : doc->as<JsonArray>()) {
      String home=m["Team1"]["TeamName"]|"";
      String away=m["Team2"]["TeamName"]|"";
      bool finished=m["MatchIsFinished"]|false;
      const char* dtStr=m["MatchDateTime"]|"";
      uint32_t kickoff=parseUTC(dtStr);
      // OL Zeit ist lokal - UTC-Offset abziehen
      if (kickoff>3600) kickoff-=3600; // Vereinfacht: immer -1h
      if (!teamMatches(home,searchKey)&&!teamMatches(away,searchKey)) continue;
      uint8_t s1=0,s2=0; bool hasScore=false;
      for (JsonObject r : m["MatchResults"].as<JsonArray>()) {
        if (String(r["ResultName"]|"")=="Aktuelles Ergebnis") {
          s1=r["PointsTeam1"]|0; s2=r["PointsTeam2"]|0; hasScore=true; break;
        }
      }
      bool isLive=(!finished&&kickoff>0&&kickoff<now&&hasScore);
      if (scheduleMode) {
        if (isLive) { res.found=true; res.isLive=true; res.kickoffUTC=kickoff; break; }
        if (!finished&&kickoff>now&&kickoff<now+86400) { res.found=true; res.kickoffUTC=kickoff; break; }
      } else {
        res.found=true; res.isLive=isLive; res.isFinished=finished;
        res.kickoffUTC=kickoff;
        strncpy(res.homeTeam,home.c_str(),47);
        strncpy(res.awayTeam,away.c_str(),47);
        res.homeScore=s1; res.awayScore=s2;
        break;
      }
    }
  }
  delete doc;
  return res;
}

// ══════════════════════════════════════════════════════════════════════════════
class UsermodHSVTicker : public Usermod {
private:
  bool          _enabled=true;
  char          _token[48]="";
  bool          _tokenLoaded=false;
  bool          _tlsOk=false;

  uint16_t      _activePreset=0;
  int           _leagueIndex=0;
  int           _foundLeagueId=0;
  bool          _scheduleFound=false;
  uint32_t      _nextKickoffUTC=0;
  bool          _pollingActive=false;

  // Spielstand
  char          _homeTeam[48]="";
  char          _awayTeam[48]="";
  uint8_t       _homeScore=0, _awayScore=0;
  bool          _isLive=false, _isPaused=false, _isFinished=false;
  bool          _myTeamIsHome=true;

  // Tor
  bool          _goalActive=false, _goalMyTeam=false;
  unsigned long _goalStart=0;

  // Timing
  unsigned long _lastPoll=0;
  unsigned long _lastSchedule=0;
  unsigned long _lastRequest=0;

  // Debug
  int           _lastHttpCode=0;
  int           _lastJsonLen=0;
  char          _lastUrl[80]="";
  char          _lastBody[64]="";

  static const unsigned long MIN_REQUEST_GAP = 7000UL;

  bool canRequest() {
    return (millis()-_lastRequest)>=MIN_REQUEST_GAP;
  }

  void loadToken() {
    if (!WLED_FS.exists("/cfg.json")) return;
    File f=WLED_FS.open("/cfg.json","r");
    if (!f) return;
    size_t sz=f.size();
    if (sz>32768) { f.close(); return; }
    char* buf=(char*)malloc(sz+1);
    if (!buf) { f.close(); return; }
    f.readBytes(buf,sz); buf[sz]=0; f.close();
    const char* tk=strstr(buf,"HSVTicker");
    if (tk) {
      const char* tv=strstr(tk,"\"token\"");
      if (tv) {
        const char* ts=strchr(tv+7,'"');
        if (ts) { ts++; const char* te=strchr(ts,'"');
          if (te&&te>ts&&(size_t)(te-ts)<sizeof(_token)) {
            size_t len=(size_t)(te-ts); strncpy(_token,ts,len); _token[len]=0; } }
      }
    }
    free(buf);
  }

  // ── Einen Request machen und Ergebnis zurückgeben ──────────────────────────
  MatchResult doRequest(int leagueId, bool scheduleMode) {
    _lastRequest=millis();
    bool isOL=(leagueId<0);
    String sk=getSearchKey(_activePreset);
    MatchResult res;

    // Buffer dynamisch
    size_t heap=ESP.getMaxAllocHeap();
    size_t bufSize=(heap>60000)?32768:(heap>35000)?16384:8192;
    char* buf=(char*)malloc(bufSize);
    if (!buf) { _lastHttpCode=-99; return res; }
    buf[0]=0;

    int code=0;

    if (isOL) {
      // OpenLigaDB Kürzel
      const char* olNames[]={"bl1","bl2","bl3","blrel","dfb-pokal"};
      int idx=(-leagueId)-1;
      if (idx<0||idx>4) { free(buf); return res; }
      snprintf(_lastUrl,sizeof(_lastUrl),"OL/%s",olNames[idx]);
      code=olRequest(olNames[idx],buf,bufSize);
      if (code==200) res=parseOL(buf,sk.c_str(),scheduleMode);
    } else {
      // fd.org
      char path[128];
      if (leagueId==FD_WC||leagueId==FD_EC) {
        const char* code2=(leagueId==FD_WC)?"WC":"EC";
        if (scheduleMode) {
          time_t n=time(nullptr); struct tm* t=gmtime(&n);
          char df[12]; snprintf(df,sizeof(df),"%04d-%02d-%02d",t->tm_year+1900,t->tm_mon+1,t->tm_mday);
          snprintf(path,sizeof(path),"/v4/competitions/%s/matches?dateFrom=%s&dateTo=%s&status=TIMED,IN_PLAY,PAUSED,SCHEDULED,FINISHED",code2,df,df);
        } else {
          snprintf(path,sizeof(path),"/v4/competitions/%s/matches?status=IN_PLAY,PAUSED,FINISHED",code2);
        }
      } else {
        int season=2025; // calcSeason würde hier stehen
        time_t n=time(nullptr); struct tm* t=gmtime(&n);
        int yr=t->tm_year+1900, mo=t->tm_mon+1;
        season=(mo>=8)?yr:yr-1;
        if (scheduleMode)
          snprintf(path,sizeof(path),"/v4/competitions/%d/matches?season=%d&status=TIMED,SCHEDULED,IN_PLAY,PAUSED",leagueId,season);
        else
          snprintf(path,sizeof(path),"/v4/competitions/%d/matches?season=%d&status=IN_PLAY,PAUSED,FINISHED",leagueId,season);
      }
      snprintf(_lastUrl,sizeof(_lastUrl),"FD%s",path+16); // Kürzen für Debug
      code=fdRequest(path,_token,buf,bufSize);
      if (code==200) res=parseFD(buf,sk.c_str(),scheduleMode);
    }

    _lastHttpCode=code;
    _lastJsonLen=(int)strlen(buf);
    strncpy(_lastBody,buf,63); _lastBody[63]=0;

    // Log
    char logline[120];
    snprintf(logline,sizeof(logline),"HTTP%d %db %s found=%d",
      code,_lastJsonLen,_lastUrl,res.found);
    writeLog(logline);

    free(buf);
    return res;
  }

  // ── Schedule: Nächstes Spiel suchen ──────────────────────────────────────
  void doSchedule() {
    LeagueChain chain=getLeagueChain(_activePreset);
    for (int i=_leagueIndex;i<4;i++) {
      int lid=chain.ids[i];
      if (lid==0) break;
      if (!canRequest()) return;
      MatchResult res=doRequest(lid,true);
      if (res.found) {
        _foundLeagueId=lid;
        _nextKickoffUTC=res.kickoffUTC;
        _scheduleFound=true;
        _leagueIndex=0;
        _lastSchedule=millis();
        if (res.isLive) { _pollingActive=true; _lastPoll=0; }
        return;
      }
      _leagueIndex=i+1;
      delay(100);
    }
    // Alle Ligen durch - nächste Suche in 5 Min
    _leagueIndex=0;
    _lastSchedule=millis();
  }

  // ── Live-Polling: Spielstand abfragen ─────────────────────────────────────
  void doLive() {
    if (!_foundLeagueId) return;
    MatchResult res=doRequest(_foundLeagueId,false);
    if (!res.found) return;

    // Spielstand speichern
    strncpy(_homeTeam,res.homeTeam,47);
    strncpy(_awayTeam,res.awayTeam,47);
    _isLive=res.isLive;
    _isPaused=res.isPaused;
    _isFinished=res.isFinished;
    if (_nextKickoffUTC==0&&res.kickoffUTC>0) _nextKickoffUTC=res.kickoffUTC;

    // Mein Team Heimspieler?
    String sk=getSearchKey(_activePreset);
    _myTeamIsHome=teamMatches(String(_homeTeam),sk.c_str());

    uint8_t totalGoals=res.homeScore+res.awayScore;

    if (res.isFinished) {
      _homeScore=res.homeScore; _awayScore=res.awayScore;
      _pollingActive=false;
    } else if (res.isLive||res.isPaused) {
      if (!_goalActive&&totalGoals>_homeScore+_awayScore) {
        // Tor!
        bool homeScored=(res.homeScore>_homeScore);
        bool myGoal=(_myTeamIsHome&&homeScored)||(!_myTeamIsHome&&!homeScored);
        _goalActive=true; _goalMyTeam=myGoal; _goalStart=millis();
        applyPreset(myGoal?PRESET_GOAL_MINE:PRESET_GOAL_OPPONENT);
      }
      _homeScore=res.homeScore; _awayScore=res.awayScore;
    }
  }

  bool isMatchTime() {
    if (!_nextKickoffUTC) return false;
    uint32_t n=(uint32_t)time(nullptr);
    return (n>=_nextKickoffUTC-300&&n<=_nextKickoffUTC+7200);
  }

  void resetState() {
    _leagueIndex=0; _foundLeagueId=0; _scheduleFound=false;
    _nextKickoffUTC=0; _pollingActive=false;
    _goalActive=false; _isLive=false; _isPaused=false; _isFinished=false;
    _homeScore=0; _awayScore=0;
    _homeTeam[0]=0; _awayTeam[0]=0;
    _lastSchedule=0; _lastPoll=0;
  }

public:
  void setup() override {}

  void loop() override {
    if (millis()<BOOT_DELAY_MS) return;

    if (!_tokenLoaded) {
      loadToken(); _tokenLoaded=true;
      // TLS-Test
      WiFiClientSecure t; t.setInsecure();
      _tlsOk=t.connect("clients3.google.com",443);
      t.stop();
      // NTP
      if (time(nullptr)<100000) configTime(3600,3600,"pool.ntp.org");
    }

    if (!_enabled||!WLED_CONNECTED) return;

    uint16_t cp=currentPreset;

    // Preset-Wechsel
    if (cp!=_activePreset) {
      if (cp==PRESET_REFRESH) { resetState(); applyPreset(_activePreset); return; }
      if (cp<PRESET_REFRESH||cp==PRESET_OFFLINE||
          cp==PRESET_GOAL_MINE||cp==PRESET_GOAL_OPPONENT) return;
      _activePreset=cp;
      resetState();
      return;
    }

    if (_activePreset<PRESET_REFRESH||_activePreset==PRESET_OFFLINE) return;

    unsigned long now=millis();

    // Tor-Preset Ende
    if (_goalActive&&(now-_goalStart>=GOAL_DURATION_MS)) {
      _goalActive=false; applyPreset(_activePreset);
    }
    // Tor-Preset alle 5s wiederholen
    if (_goalActive&&(now-_goalStart)%5000<100) {
      applyPreset(_goalMyTeam?PRESET_GOAL_MINE:PRESET_GOAL_OPPONENT);
    }

    // Polling starten wenn Anpfiff
    if (!_pollingActive&&_scheduleFound&&isMatchTime()) {
      _pollingActive=true; _lastPoll=0;
    }

    // Live-Polling
    if (_pollingActive&&canRequest()) {
      unsigned long interval=_isPaused?60000UL:POLL_LIVE_MS;
      if ((now-_lastPoll)>=interval) {
        _lastPoll=now;
        doLive();
        return;
      }
    }

    // Schedule-Suche - sofort beim ersten Mal, dann alle 5 Min
    if (!_scheduleFound&&canRequest()&&
        (_lastSchedule==0||(now-_lastSchedule)>=POLL_SCHED_MS)) {
      doSchedule();
    }
  }

  void addToConfig(JsonObject& root) override {
    JsonObject top=root.createNestedObject(F("HSVTicker"));
    top[F("enabled")]=_enabled;
    top[F("token")]=_token;
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
    oappend(SET_F("addInfo('HSVTicker:token',1,'football-data.org Token');"));
  }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject u=root[F("u")];
    if (u.isNull()) u=root.createNestedObject(F("u"));

    if (!_enabled) { u.createNestedArray(F("Ticker")).add("Deaktiviert"); return; }
    if (_activePreset<PRESET_REFRESH) { u.createNestedArray(F("Ticker")).add("Pausiert"); return; }

    // Status
    char st[48]="";
    if (_goalActive) {
      snprintf(st,sizeof(st),_goalMyTeam?"TOR! (60s)":"Gegentor (60s)");
    } else if (_isFinished) {
      snprintf(st,sizeof(st),"Abpfiff");
    } else if (_isPaused) {
      snprintf(st,sizeof(st),"Halbzeit %u:%u",_homeScore,_awayScore);
    } else if (_isLive) {
      snprintf(st,sizeof(st),"LIVE %u:%u",_homeScore,_awayScore);
    } else if (_scheduleFound&&_nextKickoffUTC) {
      int32_t ml=((int32_t)_nextKickoffUTC-(int32_t)time(nullptr))/60;
      if (ml>60)    snprintf(st,sizeof(st),"Anpfiff in %dh %dmin",ml/60,ml%60);
      else if (ml>0) snprintf(st,sizeof(st),"Anpfiff in %d Min",ml);
      else           snprintf(st,sizeof(st),"Spiel laeuft (warte auf Live)");
    } else if (_scheduleFound) {
      snprintf(st,sizeof(st),"Kein Spiel gefunden");
    } else {
      snprintf(st,sizeof(st),"Suche Spiel...");
    }
    u.createNestedArray(F("Status")).add(st);

    // Spiel
    if (_homeTeam[0]) {
      char s[64];
      snprintf(s,sizeof(s),"%s %u:%u %s",_homeTeam,_homeScore,_awayScore,_awayTeam);
      u.createNestedArray(F("Spiel")).add(s);
    }

    // Debug
    char dbg[80];
    snprintf(dbg,sizeof(dbg),"P%d LID%d HTTP%d T:%lu",
      _activePreset,_foundLeagueId,_lastHttpCode,(unsigned long)time(nullptr));
    u.createNestedArray(F("Debug")).add(dbg);

    // Heap
    char heap[32];
    snprintf(heap,sizeof(heap),"Heap:%u/%u",ESP.getFreeHeap(),ESP.getMaxAllocHeap());
    u.createNestedArray(F("Heap")).add(heap);

    // URL + JSON
    if (_lastUrl[0]) {
      u.createNestedArray(F("URL")).add(_lastUrl);
      char jinfo[32];
      snprintf(jinfo,sizeof(jinfo),"JSON:%db",_lastJsonLen);
      u.createNestedArray(F("JSON")).add(jinfo);
    }

    // Log
    if (WLED_FS.exists("/hsv_log.txt")) {
      File lf=WLED_FS.open("/hsv_log.txt","r");
      if (lf) {
        char linfo[32]; snprintf(linfo,sizeof(linfo),"Log:%dB /hsv_log.txt",(int)lf.size());
        u.createNestedArray(F("Log")).add(linfo);
        lf.close();
      }
    }

    // TLS
    u.createNestedArray(F("TLS")).add(_tlsOk?"ok":"fail");
  }

  uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }
};
#endif