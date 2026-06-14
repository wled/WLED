#pragma once
/*
 * UsermodHSVTicker - WLED Usermod v2
 * Fussball-Liveticker
 * Primaer:  football-data.org (Token, Raw TCP HTTPS - kein HTTPClient)
 * Fallback: OpenLigaDB (kein Token, nur deutsche Ligen, HTTP)
 *
 * Raw TCP fuer fd.org umgeht das 307-Redirect-Problem von HTTPClient
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

#define OL_BL1      -1
#define OL_BL2      -2
#define OL_BL3      -3
#define OL_BLREL    -4
#define OL_DFB      -5
#define OL_WM       -6
#define OL_EM       -7

static const char* olLeague(int idx) {
  switch(idx) {
    case OL_BL1:   return "bl1";
    case OL_BL2:   return "bl2";
    case OL_BL3:   return "bl3";
    case OL_BLREL: return "blrel";
    case OL_DFB:   return "dfb-pokal";
    case OL_WM:    return "wm";
    case OL_EM:    return "em";
    default:       return "bl1";
  }
}

static int calcSeason(bool isWM=false, bool isEM=false) {
  time_t n=time(nullptr);
  if (n<100000) return 2025;
  struct tm* t=gmtime(&n);
  int year=t->tm_year+1900, month=t->tm_mon+1;
  if (isWM) {
    // WM: 2026 laeuft gerade, danach 2030
    if (year<=2026) return 2026;
    return 2026+((year-2026+3)/4)*4;
  }
  if (isEM) {
    // EM: 2024 war die letzte, danach 2028
    if (year<=2024) return 2024;
    if (year<=2028) return 2024; // Noch aktuelle Saison in fd.org
    return 2028+((year-2028+3)/4)*4;
  }
  return (month>=8)?year:year-1;
}

struct LeagueChain { int ids[6]; int seasons[6]; };

static LeagueChain getLeagueChain(uint16_t pid) {
  int ls=calcSeason(), wm=calcSeason(true), em=calcSeason(false,true);
  if (pid==PRESET_GERMANY)
    // Deutschland WM 2026: direkt fd.org WC (OL hat keine WM 2026 Daten)
    return {{FD_WC,FD_EC,0,0,0,0},{wm,em,0,0,0,0}};
  if (pid>=PRESET_BL_START    && pid<=PRESET_BL_END)
    return {{OL_BL1,OL_BL2,OL_BL3,OL_BLREL,OL_DFB,0},{0,0,0,0,0,0}};
  if (pid>=PRESET_PL_START    && pid<=PRESET_PL_END)
    return {{FD_PL,FD_ELC,0,0,0,0},{ls,ls,0,0,0,0}};
  if (pid>=PRESET_LALIGA_START && pid<=PRESET_LALIGA_END)
    return {{FD_PD,FD_SD,0,0,0,0},{ls,ls,0,0,0,0}};
  if (pid>=PRESET_LIGUE1_START && pid<=PRESET_LIGUE1_END)
    return {{FD_FL1,FD_FL2,0,0,0,0},{ls,ls,0,0,0,0}};
  if (pid>=PRESET_SERIEA_START && pid<=PRESET_SERIEA_END)
    return {{FD_SA,FD_SB,0,0,0,0},{ls,ls,0,0,0,0}};
  return {{0,0,0,0,0,0},{0,0,0,0,0,0}};
}

struct ClubEntry { char key[32]; uint16_t presetId; };

static const ClubEntry ALL_CLUBS[] PROGMEM = {
  {"hamburg",154},
  {"fürth|fuerth",155},{"greuther",155},{"greuter",155},
  {"bayern",156},{"dortmund",157},{"leverkusen",158},{"leipzig",159},
  {"frankfurt",160},{"wolfsburg",161},{"freiburg",162},{"augsburg",163},
  {"mainz",164},{"hoffenheim",165},{"gladbach",166},{"köln|koeln",167},
  {"union berlin",168},{"union",168},{"bochum",169},{"heidenheim",170},
  {"pauli",171},{"kiel",172},{"hertha",173},{"schalke",174},
  {"kaisers",175},{"nürnberg|nuernberg",176},{"darmstadt",177},{"hannover",178},
  {"düsseldorf|duesseldorf",179},{"magdeburg",180},{"braunschweig",181},
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

  void logMsg(const char* msg) {
    // Log in /hsv_log.txt - max 4KB, dann rotieren
    File f=WLED_FS.open("/hsv_log.txt","r");
    size_t sz=f?f.size():0; if(f) f.close();
    const char* mode=(sz>3800)?"w":"a"; // rotieren wenn >3.8KB
    f=WLED_FS.open("/hsv_log.txt",mode);
    if (!f) return;
    // Zeitstempel
    uint32_t t=(uint32_t)time(nullptr);
    char line[128];
    snprintf(line,sizeof(line),"[%lu] %s\n",(unsigned long)t,msg);
    f.print(line);
    f.close();
  }
  char _token[48]="";
  bool _tokenLoaded=false;

  uint16_t   _activePreset=0,_prevPreset=0;
  MatchState _matchState=MatchState::IDLE;
  MatchInfo  _matchInfo;
  bool       _myTeamIsHome=true;
  uint8_t    _lastGoals=0,_homeScore=0,_awayScore=0;

  int  _leagueIndex=0,_foundLeagueId=0,_currentMatchday=-1;

  static const int MAX_BLOCKED=4;
  int           _blockedIds[4]={0,0,0,0};
  unsigned long _blockedTime[4]={0,0,0,0};
  static const unsigned long BLOCK_DURATION=86400000UL;

  bool          _scheduleFetched=false;
  unsigned long _lastScheduleFetch=0;
  uint32_t      _nextKickoffUTC=0;
  volatile bool  _scheduleFound=false; // sofort im Task gesetzt
  bool          _pollingActive=false;
  bool          _goalActive=false,_goalMyTeam=false;
  unsigned long _goalStart=0,_lastPoll=0;
  int           _lastHttpCode=0;
  bool          _tlsWorks=false;
  unsigned long _taskStarted=0,_lastRequestTime=0;

  static const unsigned long GOAL_DURATION   = 60000UL;
  static const unsigned long POLL_INTERVAL   = 30000UL;
  static const unsigned long SCHED_INTERVAL  = 86400000UL;
  static const unsigned long BOOT_DELAY      = 30000UL;
  static const unsigned long MIN_REQUEST_GAP = 7000UL;
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
    bool isOpenLiga=false;
    char debugUrl[64]={};
    int jsonLen=0;
    bool jsonTrunc=false;
    DeserializationError jsonErr=DeserializationError::Ok;
    char jsonSnip[64]={};  // Erste 63 Zeichen des JSON-Bodys
  };
  TaskResult _taskResult;

  struct TaskParam {
    char url[256],token[48],searchKey[32],fdPath[200];
    FetchType type;
    bool isOpenLiga;
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

  static uint32_t parseOLDate(const char* s) {
    uint32_t ts=parseUTC(s);
    if (ts==0) return 0;
    int mo=0; sscanf(s+5,"%d",&mo);
    uint32_t offset=(mo>=4&&mo<=9)?7200:3600;
    return (ts>offset)?ts-offset:0;
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

  void loadTokenFromFile() {
    if (!WLED_FS.exists("/cfg.json")) return;
    File f=WLED_FS.open("/cfg.json","r");
    if (!f) return;
    size_t sz=f.size();
    if (sz>32768) { f.close(); return; }
    char* buf=(char*)malloc(sz+1);
    if (!buf) { f.close(); return; }
    f.readBytes(buf,sz);
    buf[sz]=0;
    f.close();
    const char* tk=strstr(buf,"HSVTicker");
    if (tk) {
      const char* tv=strstr(tk,"\"token\"");
      if (tv) {
        const char* ts=strchr(tv+7,'"');
        if (ts) {
          ts++;
          const char* te=strchr(ts,'"');
          if (te&&te>ts&&(size_t)(te-ts)<sizeof(_token)) {
            size_t len=(size_t)(te-ts);
            strncpy(_token,ts,len);
            _token[len]=0;
          }
        }
      }
      const char* ev=strstr(tk,"\"enabled\"");
      if (ev) {
        const char* es=ev+9;
        while (*es&&(*es==':'||*es==' ')) es++;
        _enabled=(strncmp(es,"true",4)==0);
      }
    }
    free(buf);
  }

  // ── Fetch Task ──────────────────────────────────────────
  static void fetchTask(void* pv) {
    TaskParam* p=(TaskParam*)pv;
    UsermodHSVTicker* self=p->self;
    // Buffer-Größe: konservativ wegen Task-Stack (14KB) und JsonDoc
    size_t heapFree=ESP.getMaxAllocHeap();
    size_t bufSize=0;
    if (heapFree>80000)      bufSize=32768;
    else if (heapFree>55000) bufSize=16384;
    else if (heapFree>35000) bufSize=8192;
    else                     bufSize=4096;
    char* buf=(char*)malloc(bufSize);
    bool ok=false;
    int httpCode=0;

    if (!buf) {
      // malloc fehlgeschlagen - zu wenig Heap
      httpCode=-99; // kein Speicher
    } else if (WiFi.status()==WL_CONNECTED) {
      if (p->isOpenLiga) {
        // OpenLigaDB: HTTPS (HTTP gibt 307)
        WiFiClientSecure olClient;
        olClient.setInsecure();
        HTTPClient http;
        http.setConnectTimeout(8000);
        http.setTimeout(12000);
        http.setReuse(false);
        if (http.begin(olClient,p->url)) {
          http.addHeader("Accept","application/json");
          httpCode=http.GET();
          // httpCode=-1 = connect fail, -11 = timeout
          if (httpCode==HTTP_CODE_OK) {
            String body=http.getString();
            size_t len=body.length();
            if (len>0&&len<(int)bufSize-1) { memcpy(buf,body.c_str(),len+1); ok=(len>10); }
            if (len>=(int)bufSize-1) { memcpy(buf,body.c_str(),bufSize-1); buf[bufSize-1]=0; ok=true; } // Truncated
          } else if (httpCode<=0) {
            // httpCode bleibt negativ (z.B. -1=connect, -11=timeout)
          }
          http.end();
        }
      } else {
        // football-data.org: Raw TCP auf Port 443
        // Umgeht HTTPClient Redirect-Problem (307)
        WiFiClientSecure* sc=new WiFiClientSecure();
        if (sc) {
          sc->setInsecure();
          sc->setTimeout(30);
          sc->setHandshakeTimeout(30);
          if (sc->connect("api.football-data.org",443)) {
            sc->print("GET ");
            sc->print(p->fdPath);
            sc->println(" HTTP/1.1");
            sc->println("Host: api.football-data.org");
            sc->print("X-Auth-Token: ");
            sc->println(p->token);
            sc->println("Accept: application/json");
            sc->println("Connection: close");
            sc->println();
            // Header lesen und Status-Code extrahieren
            unsigned long t=millis();
            bool headerDone=false;
            bool chunked=false;
            int r=0;
            char hbuf[512];
            int hlen=0;
            while ((millis()-t)<15000&&(sc->connected()||sc->available())) {
              if (sc->available()) {
                char c=(char)sc->read();
                if (!headerDone) {
                  if (hlen<510) hbuf[hlen++]=c;
                  hbuf[hlen]=0;
                  if (hlen>=4 &&
                      hbuf[hlen-4]=='\r' && hbuf[hlen-3]=='\n' &&
                      hbuf[hlen-2]=='\r' && hbuf[hlen-1]=='\n') {
                    headerDone=true;
                    const char* hs=strstr(hbuf,"HTTP/1.");
                    if (hs) httpCode=atoi(hs+9);
                    chunked=(strstr(hbuf,"Transfer-Encoding: chunked")!=nullptr||
                             strstr(hbuf,"transfer-encoding: chunked")!=nullptr);
                  }
                } else if (chunked) {
                  // Chunked Transfer Encoding dekodieren
                  // c = erstes Zeichen der Chunk-Size-Zeile
                  char cline[16]; int cl=0;
                  cline[cl++]=c;
                  unsigned long ct2=millis();
                  while ((millis()-ct2)<2000) {
                    if (sc->available()) {
                      char cc=(char)sc->read();
                      if (cl<15) cline[cl++]=cc;
                      if (cl>=2&&cline[cl-2]=='\r'&&cline[cl-1]=='\n') break;
                    } else delay(1);
                  }
                  cline[cl]=0;
                  int chunkSize=(int)strtol(cline,nullptr,16);
                  if (chunkSize==0) break;
                  // Chunk-Daten lesen
                  int got=0;
                  unsigned long ct=millis();
                  while (got<chunkSize&&(millis()-ct)<8000) {
                    if (sc->available()) {
                      char cd=(char)sc->read();
                      if (r<(int)bufSize-1) buf[r++]=cd;
                      got++;
                    } else delay(1);
                  }
                  buf[r]=0;
                  // trailing \r\n nach Chunk
                  ct=millis();
                  int crlfCount=0;
                  while (crlfCount<2&&(millis()-ct)<1000) {
                    if (sc->available()) {
                      char cd=(char)sc->read();
                      if (cd=='\r'||cd=='\n') crlfCount++;
                      else break;
                    } else delay(1);
                  }
                } else {
                  if (r<(int)bufSize-1) buf[r++]=c;
                }
              } else delay(1);
            }
            buf[r]=0;
            ok=(r>10&&httpCode==200);
          } else {
            httpCode=-1; // connect() fehlgeschlagen
          }
          delete sc;
        }
      }
    }

    xSemaphoreTake(self->_mutex,portMAX_DELAY);
    // Log schreiben
    char logbuf[96];
    snprintf(logbuf,sizeof(logbuf),"HTTP%d %db%s url=%s",
      httpCode, buf?(int)strlen(buf):0,
      (buf&&strlen(buf)>=(int)bufSize-2)?" TRUNC":"",
      p->url+20); // url ohne https://api.football-data.org
    self->logMsg(logbuf);
    self->_taskResult.httpCode=httpCode;
    self->_taskResult.isOpenLiga=p->isOpenLiga;
    self->_taskResult.debugUrl[0]=0;
    strncat(self->_taskResult.debugUrl,p->url,63);
    if (buf) {
      self->_taskResult.jsonLen=strlen(buf);
      self->_taskResult.jsonTrunc=(self->_taskResult.jsonLen>=(int)bufSize-2);
      strncpy(self->_taskResult.jsonSnip,buf,63);
      self->_taskResult.jsonSnip[63]=0;
    }

    if (ok && buf) {
      if (!p->isOpenLiga) {
        self->_taskResult.currentMatchday=extractMatchday(buf);
        parseFDOrg(buf,p->searchKey,p->type,self->_taskResult);
      } else {
        parseOpenLiga(buf,p->searchKey,p->type,self->_taskResult);
      }
    }

    // Parse-Ergebnis loggen
    char plog[80];
    snprintf(plog,sizeof(plog),"parse: found=%d kickoff=%lu lid=%s",
      self->_taskResult.teamFound,
      (unsigned long)self->_taskResult.nextKickoffUTC,
      p->isOpenLiga?"OL":"FD");
    self->logMsg(plog);
    self->_taskResult.type=p->type;
    // Wenn Spiel bereits gefunden: ignorieren
    if (self->_scheduleFound && self->_taskResult.type==FetchType::SCHEDULE) {
      self->_taskResult.teamFound=false;
      self->_taskResult.nextKickoffUTC=0;
    }
    // Sofort Flag setzen wenn jetzt gefunden
    if (self->_taskResult.teamFound && self->_taskResult.nextKickoffUTC>0) {
      self->_scheduleFound=true;
    }
    self->_taskDone=true;
    self->_taskRunning=false; // VOR Give - verhindert Race Condition im Hauptloop
    xSemaphoreGive(self->_mutex);
    free(buf); free(p);
    vTaskDelete(NULL);
  }


static bool teamMatches(const String& teamName, const char* searchKey) {
  String name = teamName; name.toLowerCase();
  String keys = String(searchKey); keys.toLowerCase();
  int start = 0;
  int sep = keys.indexOf('|');
  while (true) {
    String key = (sep>=0) ? keys.substring(start,sep) : keys.substring(start);
    key.trim();
    if (key.length()>0 && name.indexOf(key)>=0) return true;
    if (sep<0) break;
    start = sep+1;
    sep = keys.indexOf('|',start);
  }
  return false;
}
  static void parseFDOrg(const char* buf,const char* searchKey,
                          FetchType type,TaskResult& res) {
    StaticJsonDocument<512> filter;
    filter["matches"][0]["utcDate"]                   =true;
    filter["matches"][0]["status"]                    =true;
    filter["matches"][0]["homeTeam"]["name"]          =true;
    filter["matches"][0]["awayTeam"]["name"]          =true;
    filter["matches"][0]["score"]["fullTime"]["home"] =true;
    filter["matches"][0]["score"]["fullTime"]["away"] =true;

    size_t docSize=16384; // fest - bufSize ist nicht verfuegbar hier
    DynamicJsonDocument* doc=new DynamicJsonDocument(docSize);
    if (!doc) return;
    res.jsonErr=deserializeJson(*doc,buf,DeserializationOption::Filter(filter));
    res.jsonLen=strlen(buf);
    if (!res.jsonErr) {
      String search=String(searchKey); search.toLowerCase();
      uint32_t nowUTC=(uint32_t)time(nullptr);
      uint32_t bestKickoff=0; bool liveNow=false;
      for (JsonObject m : (*doc)["matches"].as<JsonArray>()) {
        String home=m["homeTeam"]["name"]|"";
        String away=m["awayTeam"]["name"]|"";
        String status=m["status"]|"";
        const char* utcDate=m["utcDate"]|"";
        uint32_t kickoff=parseUTC(utcDate);
        String hLow=home; hLow.toLowerCase();
        String aLow=away; aLow.toLowerCase();
        bool myTeam=(teamMatches(home,searchKey)||teamMatches(away,searchKey));
        if (type==FetchType::SCHEDULE) {
          if (!myTeam) continue;
          if (status=="IN_PLAY"||status=="PAUSED") {
            bestKickoff=(kickoff>0)?kickoff:nowUTC-3600;
            liveNow=true; break;
          }
          if (kickoff>nowUTC&&kickoff<nowUTC+86400*7) {
            if (!bestKickoff||kickoff<bestKickoff) bestKickoff=kickoff;
          }
        } else {
          if (!myTeam) continue;
          res.match.valid=true; res.match.kickoffUTC=kickoff;
          strncpy(res.match.homeTeam,home.c_str(),sizeof(res.match.homeTeam)-1);
          strncpy(res.match.awayTeam,away.c_str(),sizeof(res.match.awayTeam)-1);
          res.match.isLive=(status=="IN_PLAY"||status=="PAUSED");
          res.match.isFinished=(status=="FINISHED");
          int sh=m["score"]["fullTime"]["home"]|-1;
          int sa=m["score"]["fullTime"]["away"]|-1;
          if (sh>=0) res.match.homeScore=(uint8_t)sh;
          if (sa>=0) res.match.awayScore=(uint8_t)sa;
          res.teamFound=true; res.success=true; break;
        }
      }
      if (type==FetchType::SCHEDULE) {
        res.nextKickoffUTC=bestKickoff;
        res.teamFound=(bestKickoff>0);
        res.matchIsLiveNow=liveNow;
        res.success=true;
      }
    }
    delete doc;
  }

  static void parseOpenLiga(const char* buf,const char* searchKey,
                             FetchType type,TaskResult& res) {
    StaticJsonDocument<512> filter;
    JsonArray fa=filter.to<JsonArray>();
    JsonObject fo=fa.createNestedObject();
    fo["MatchDateTime"]                        =true;
    fo["MatchIsFinished"]                      =true;
    fo["Team1"]["TeamName"]                    =true;
    fo["Team2"]["TeamName"]                    =true;
    fo["MatchResults"][0]["ResultName"]        =true;
    fo["MatchResults"][0]["PointsTeam1"]       =true;
    fo["MatchResults"][0]["PointsTeam2"]       =true;

    size_t docSize=16384; // fest - bufSize ist nicht verfuegbar hier
    DynamicJsonDocument* doc=new DynamicJsonDocument(docSize);
    if (!doc) return;
    res.jsonErr=deserializeJson(*doc,buf,DeserializationOption::Filter(filter));
    res.jsonLen=strlen(buf);
    if (!res.jsonErr) {
      String search=String(searchKey); search.toLowerCase();
      uint32_t nowUTC=(uint32_t)time(nullptr);
      uint32_t bestKickoff=0; bool liveNow=false;
      for (JsonObject m : doc->as<JsonArray>()) {
        String home=m["Team1"]["TeamName"]|"";
        String away=m["Team2"]["TeamName"]|"";
        bool finished=m["MatchIsFinished"]|false;
        const char* dtStr=m["MatchDateTime"]|"";
        uint32_t kickoff=parseOLDate(dtStr);
        String hLow=home; hLow.toLowerCase();
        String aLow=away; aLow.toLowerCase();
        bool myTeam=(teamMatches(home,searchKey)||teamMatches(away,searchKey));
        uint8_t score1=0,score2=0; bool hasLive=false;
        for (JsonObject r : m["MatchResults"].as<JsonArray>()) {
          String rn=r["ResultName"]|"";
          if (rn=="Aktuelles Ergebnis") {
            score1=r["PointsTeam1"]|0;
            score2=r["PointsTeam2"]|0;
            hasLive=true; break;
          }
        }
        bool isLive=(!finished&&kickoff>0&&kickoff<nowUTC&&hasLive);
        bool isToday=(kickoff>nowUTC-7200&&kickoff<nowUTC+86400); // max 2h vergangen
        bool isFuture=(kickoff>nowUTC&&kickoff<nowUTC+86400*7);   // naechste 7 Tage
        if (type==FetchType::SCHEDULE) {
          if (!myTeam) continue;
          if (isLive) { bestKickoff=(kickoff>0)?kickoff:nowUTC-3600; liveNow=true; break; }
          if (isFuture||isToday) { if (!bestKickoff||kickoff<bestKickoff) bestKickoff=kickoff; }
        } else {
          if (!myTeam) continue;
          res.match.valid=true; res.match.kickoffUTC=kickoff;
          strncpy(res.match.homeTeam,home.c_str(),sizeof(res.match.homeTeam)-1);
          strncpy(res.match.awayTeam,away.c_str(),sizeof(res.match.awayTeam)-1);
          res.match.isLive=isLive;
          res.match.isFinished=finished;
          res.match.homeScore=score1;
          res.match.awayScore=score2;
          res.teamFound=true; res.success=true; break;
        }
      }
      if (type==FetchType::SCHEDULE) {
        res.nextKickoffUTC=bestKickoff;
        res.teamFound=(bestKickoff>0);
        res.matchIsLiveNow=liveNow;
        res.success=true;
      }
    }
    delete doc;
  }

  enum class Phase : uint8_t { IDLE, WAITING } _phase=Phase::IDLE;

  bool canRequest() {
    unsigned long now=millis();
    if (_lastRequestTime==0) return true;
    if (_lastHttpCode==429&&(now-_lastRequestTime)<RATE_LIMIT_WAIT) return false;
    if ((now-_lastRequestTime)<MIN_REQUEST_GAP) return false;
    return true;
  }

  bool isBlocked(int leagueId) {
    if (leagueId<=0) return false;
    unsigned long now=millis();
    for (int i=0;i<MAX_BLOCKED;i++) {
      if (_blockedIds[i]==leagueId&&(now-_blockedTime[i])<BLOCK_DURATION) return true;
    }
    return false;
  }

  void blockLeague(int leagueId) {
    if (leagueId<=0) return;
    for (int i=0;i<MAX_BLOCKED;i++) {
      if (_blockedIds[i]==0||_blockedIds[i]==leagueId) {
        _blockedIds[i]=leagueId; _blockedTime[i]=millis(); return;
      }
    }
    _blockedIds[0]=leagueId; _blockedTime[0]=millis();
  }

  void startFetch(FetchType type,int leagueId) {
    if (_taskRunning) return;
    if (!canRequest()) return;
    // Sofort taskRunning setzen - verhindert zweiten Task
    _taskRunning=true;

    TaskParam* p=(TaskParam*)malloc(sizeof(TaskParam));
    if (!p) return;

    bool isOL=(leagueId<0);
    p->isOpenLiga=isOL;

    if (isOL) {
      // Ohne Saison: API gibt automatisch aktuellen Spieltag zurück
      snprintf(p->url,sizeof(p->url),
        "https://api.openligadb.de/getmatchdata/%s",
        olLeague(leagueId));
      p->fdPath[0]=0;
      p->token[0]=0;
    } else {
      LeagueChain chain=getLeagueChain(_activePreset);
      int season=2025;
      for (int i=0;i<6;i++) if (chain.ids[i]==leagueId){season=chain.seasons[i];break;}
      // Pfad fuer Raw TCP Request
      if (type==FetchType::LIVE&&_currentMatchday>0) {
        snprintf(p->fdPath,sizeof(p->fdPath),
          "/v4/competitions/%d/matches?season=%d&matchday=%d",
          leagueId,season,_currentMatchday);
      } else if (type==FetchType::LIVE) {
        if (leagueId==FD_WC||leagueId==FD_EC) {
          // WC/EC: Kurzcode ohne Season - gibt aktuelle Saison automatisch
          const char* code2=(leagueId==FD_WC)?"WC":"EC";
          snprintf(p->fdPath,sizeof(p->fdPath),
            "/v4/competitions/%s/matches?status=IN_PLAY,PAUSED,FINISHED",code2);
        } else {
          snprintf(p->fdPath,sizeof(p->fdPath),
            "/v4/competitions/%d/matches?season=%d&status=IN_PLAY,PAUSED,FINISHED",
            leagueId,season);
        }
      } else {
        if (leagueId==FD_WC||leagueId==FD_EC) {
          const char* code2=(leagueId==FD_WC)?"WC":"EC";
          // Nur Spiele der naechsten 7 Tage abfragen - reduziert Buffer-Bedarf
          time_t n=time(nullptr); struct tm* t=gmtime(&n);
          char df[16],dt[16];
          snprintf(df,sizeof(df),"%04d-%02d-%02d",t->tm_year+1900,t->tm_mon+1,t->tm_mday);
          n+=7*86400; t=gmtime(&n);
          snprintf(dt,sizeof(dt),"%04d-%02d-%02d",t->tm_year+1900,t->tm_mon+1,t->tm_mday);
          // Nur heutigen Tag - minimiert Response-Größe
          snprintf(p->fdPath,sizeof(p->fdPath),
            "/v4/competitions/%s/matches?dateFrom=%s&dateTo=%s&status=TIMED,IN_PLAY,PAUSED,SCHEDULED",code2,df,df); // df=heute, dt ignoriert
        } else {
          snprintf(p->fdPath,sizeof(p->fdPath),
            "/v4/competitions/%d/matches?season=%d&status=SCHEDULED,IN_PLAY,PAUSED",
            leagueId,season);
        }
      }
      snprintf(p->url,sizeof(p->url),"https://api.football-data.org%s",p->fdPath);
      strncpy(p->token,_token,sizeof(p->token)-1); p->token[sizeof(p->token)-1]=0;
    }

    String sk=getSearchKey();
    strncpy(p->searchKey,sk.c_str(),sizeof(p->searchKey)-1); p->searchKey[sizeof(p->searchKey)-1]=0;
    p->type=type; p->self=this;
    _taskDone=false; _taskResult=TaskResult{};
    _taskStarted=millis(); _lastRequestTime=millis();

    if (WiFi.status()!=WL_CONNECTED) {
      _taskRunning=false; _lastHttpCode=-99; free(p); return;
    }
    if (xTaskCreatePinnedToCore(fetchTask,"hsv_fetch",14336,p,1,nullptr,0)!=pdPASS) {
      _taskRunning=false; free(p);
    }
  }

  void skipLeague() {
    _leagueIndex++;
    LeagueChain chain=getLeagueChain(_activePreset);
    if (_leagueIndex<6&&chain.ids[_leagueIndex]!=0) {
      _scheduleFetched=false;
      _lastScheduleFetch=millis()-(SCHED_INTERVAL-MIN_REQUEST_GAP-1000);
    } else {
      // Alle Ligen durch - von vorne, aber nextKickoffUTC behalten wenn bereits gefunden
      _leagueIndex=0;
      if (_nextKickoffUTC==0) _foundLeagueId=0; // nur reset wenn noch nichts gefunden
      // nextKickoffUTC bleibt erhalten - verhindert "Kein Spiel" nach Heap-Fehler
    }
  }

  void processScheduleResult(TaskResult& res) {
    _lastHttpCode=res.httpCode; // immer überschreiben
    if (res.currentMatchday>0) _currentMatchday=res.currentMatchday;

    if (res.httpCode==403) {
      int curId=getCurrentLeagueId();
      if (curId>0) blockLeague(curId);
      skipLeague(); _scheduleFetched=false; _phase=Phase::IDLE; return;
    }
    if (res.httpCode==307||res.httpCode==-1||res.httpCode==-99) {
      // TLS-Fehler: nur weitersuchen wenn noch nichts gefunden
      if (_nextKickoffUTC==0) {
        _lastScheduleFetch=millis()-(SCHED_INTERVAL-30000);
        skipLeague(); _scheduleFetched=false;
      }
      _phase=Phase::IDLE; return;
    }

    if (res.httpCode==429) { _phase=Phase::IDLE; return; }

    // Wenn bereits gefunden: alles ignorieren - kein weiteres Suchen
    if (_nextKickoffUTC>0) { _phase=Phase::IDLE; return; }

    _scheduleFetched=true; _lastScheduleFetch=millis();

    if (res.teamFound&&res.nextKickoffUTC>0) {
      _nextKickoffUTC=res.nextKickoffUTC;
      _foundLeagueId=getCurrentLeagueId();
      _leagueIndex=0;
      _scheduleFetched=true;
      _lastScheduleFetch=millis();
      _matchState=MatchState::SCHEDULED; // ← Status auf "Anpfiff in Xh" setzen
      if (res.matchIsLiveNow) { _pollingActive=true; _lastPoll=0; }
    } else {
      skipLeague();
    }
    _phase=Phase::IDLE;
  }

  void processLiveResult(TaskResult& res) {
    _lastHttpCode=res.httpCode;
    if (res.currentMatchday>0) _currentMatchday=res.currentMatchday;

    if (res.httpCode==403) {
      int curId=getCurrentLeagueId();
      if (curId>0) blockLeague(curId);
      _lastPoll=0; _phase=Phase::IDLE; return;
    }
    if (res.httpCode==307||res.httpCode==-1) {
      _lastPoll=0; _phase=Phase::IDLE; return;
    }
    if (res.httpCode==429) { _phase=Phase::IDLE; return; }

    if (!res.teamFound||!res.match.valid) { _phase=Phase::IDLE; return; }

    _matchInfo=res.match;
    String search=getSearchKey(); search.toLowerCase();
    String hLow=String(_matchInfo.homeTeam); hLow.toLowerCase();
    _myTeamIsHome=(teamMatches(String(_matchInfo.homeTeam),getSearchKey().c_str()));
    uint8_t totalGoals=_matchInfo.homeScore+_matchInfo.awayScore;

    if (_matchInfo.isFinished) {
      if (_matchState!=MatchState::FINISHED) { _matchState=MatchState::FINISHED; _pollingActive=false; }
    } else if (_matchInfo.isLive) {
      if (_matchState!=MatchState::LIVE&&!_goalActive) {
        _matchState=MatchState::LIVE;
        _homeScore=_matchInfo.homeScore; _awayScore=_matchInfo.awayScore;
        _lastGoals=totalGoals; applyPreset(_activePreset);
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
    // OpenLigaDB: "Deutschland", fd.org: "Germany"
    // Beide werden separat geprüft in parseTeam()
    return (_activePreset==PRESET_GERMANY)?"germany|deutsch|ger":"";
  }

  int getCurrentLeagueId() {
    LeagueChain c=getLeagueChain(_activePreset);
    for (int i=_leagueIndex;i<6;i++) {
      int lid=c.ids[i];
      if (lid==0) break;
      if (lid>0&&isBlocked(lid)) continue;
      if (i!=_leagueIndex) _leagueIndex=i;
      return lid;
    }
    return 0;
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

    if (!_tokenLoaded) {
      loadTokenFromFile(); _tokenLoaded=true;
      // NTP nur wenn Zeit noch nicht synchronisiert (neuer Flash)
      if (time(nullptr)<100000) configTime(3600,3600,"pool.ntp.org","time.nist.gov");
      // TLS-Selbsttest
      WiFiClientSecure t; t.setInsecure();
      _tlsWorks=t.connect("clients3.google.com",443);
      t.stop();
    }
    if (!_enabled||!WLED_CONNECTED) return;

    unsigned long now=millis();
    uint16_t cp=currentPreset;

    if (cp!=_prevPreset) {
      _prevPreset=cp;
      if (cp==PRESET_REFRESH) {
        _scheduleFetched=false; _leagueIndex=0; _nextKickoffUTC=0;
        _pollingActive=false; _matchState=MatchState::IDLE;
        _goalActive=false; _currentMatchday=-1; _foundLeagueId=0; _lastHttpCode=0;
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
        _phase=Phase::IDLE; _currentMatchday=-1; _lastHttpCode=0;
        _scheduleFound=false;
      }
    }
    if (_activePreset<PRESET_REFRESH||_activePreset==PRESET_OFFLINE) return;

    if (_goalActive&&(now-_goalStart>=GOAL_DURATION)) {
      _goalActive=false; applyPreset(_activePreset);
    }

    if (_taskRunning&&(now-_taskStarted>35000)) {
      // Task-Timeout: Task läuft zu lang (TLS-Timeout ist 30s)
      // _taskRunning bleibt true bis Task selbst fertig ist!
      // Nur Phase zurücksetzen damit loop() nicht blockiert
      _phase=Phase::IDLE;
      // NICHT _taskRunning=false setzen - Task läuft noch!
    }

    if (_taskDone) {
      xSemaphoreTake(_mutex,portMAX_DELAY);
      TaskResult res=_taskResult; _taskDone=false;
      xSemaphoreGive(_mutex);
      if (res.type==FetchType::SCHEDULE) processScheduleResult(res);
      else processLiveResult(res);
    }
    if (_taskRunning||_phase==Phase::WAITING) return;

    if (!_scheduleFetched||(now-_lastScheduleFetch>SCHED_INTERVAL)) {
      int lid=getCurrentLeagueId();
      if (lid&&canRequest()) { startFetch(FetchType::SCHEDULE,lid); _phase=Phase::WAITING; return; }
    }

    if (!_pollingActive&&_scheduleFetched&&isMatchTime()) { _pollingActive=true; _lastPoll=0; }

    unsigned long effectivePoll=(_lastHttpCode==429)?RATE_LIMIT_WAIT:POLL_INTERVAL;
    if (_pollingActive&&(now-_lastPoll>effectivePoll)&&canRequest()) {
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
    oappend(SET_F("addInfo('HSVTicker:token',1,'football-data.org Token (optional)');"));
  }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject u=root[F("u")];
    if (u.isNull()) u=root.createNestedObject(F("u"));

    if (!_enabled)                    { u.createNestedArray(F("Ticker")).add("Deaktiviert"); return; }
    if (_activePreset<PRESET_REFRESH) { u.createNestedArray(F("Ticker")).add("Pausiert"); return; }

    // Status
    char st[48]="";
    if (_goalActive) {
      snprintf(st,sizeof(st),_goalMyTeam?"TOR! (60s)":"Gegentor (60s)");
    } else {
      switch(_matchState) {
        case MatchState::LIVE:
          snprintf(st,sizeof(st),"LIVE");
          break;
        case MatchState::FINISHED:
          snprintf(st,sizeof(st),"Abpfiff");
          break;
        case MatchState::SCHEDULED: {
          int32_t ml=((int32_t)_nextKickoffUTC-(int32_t)time(nullptr))/60;
          if (ml>60)      snprintf(st,sizeof(st),"Anpfiff in %dh %dmin",ml/60,ml%60);
          else if (ml>0)  snprintf(st,sizeof(st),"Anpfiff in %d Min",ml);
          else            snprintf(st,sizeof(st),"Warte auf Anpfiff");
          break;
        }
        default:
          if (!_scheduleFetched)       snprintf(st,sizeof(st),"Suche Spiel...");
          else if (_nextKickoffUTC==0) snprintf(st,sizeof(st),"Kein Spiel gefunden");
          else                         snprintf(st,sizeof(st),"Bereit");
          break;
      }
    }
    u.createNestedArray(F("Status")).add(st);

    if (_matchState==MatchState::LIVE||_matchState==MatchState::FINISHED) {
      char s[64];
      snprintf(s,sizeof(s),"%s %u:%u %s",
        _matchInfo.homeTeam,_homeScore,_awayScore,_matchInfo.awayTeam);
      u.createNestedArray(F("Spiel")).add(s);
    }
    int curLid=getCurrentLeagueId();
    const char* api=(curLid<0)?"OpenLigaDB":"fd.org";
    uint32_t nowt=(uint32_t)time(nullptr);
    char dbg[80];
    snprintf(dbg,sizeof(dbg),"P%d LID%d HTTP%d %s [%s]%s T:%lu",
      _activePreset,curLid,_lastHttpCode,
      _pollingActive?"Poll":"Warte",api,
      _scheduleFetched?" Sched":"",
      (unsigned long)nowt);
    u.createNestedArray(F("Debug")).add(dbg);
    char heap[24];
    snprintf(heap,sizeof(heap),"Heap:%u/%u",
      ESP.getFreeHeap(),ESP.getMaxAllocHeap());
    u.createNestedArray(F("Heap")).add(heap);
    if (_taskResult.debugUrl[0]) u.createNestedArray(F("URL")).add(_taskResult.debugUrl);
    // JSON Debug
    char jdbg[48];
    const char* jerr=_taskResult.jsonErr.c_str();
    snprintf(jdbg,sizeof(jdbg),"JSON:%db%s%s",
      _taskResult.jsonLen,
      _taskResult.jsonTrunc?" TRUNC":"",
      (jerr&&jerr[0]&&jerr[0]!='O')?jerr:"");
    // Log-Datei Info
    if (WLED_FS.exists("/hsv_log.txt")) {
      File lf=WLED_FS.open("/hsv_log.txt","r");
      if (lf) {
        size_t lsz=lf.size(); lf.close();
        char linfo[32]; snprintf(linfo,sizeof(linfo),"Log:%dB /hsv_log.txt",(int)lsz);
        u.createNestedArray(F("Log")).add(linfo);
      }
    }
    if (_taskResult.jsonLen>0) {
      u.createNestedArray(F("JSON")).add(jdbg);
      // Ersten 60 Zeichen des JSON anzeigen wenn kein Spiel gefunden
      if (!_scheduleFetched||_nextKickoffUTC==0) {
        char jsnip[64]={};
        strncpy(jsnip,_taskResult.jsonSnip,63);
        if (jsnip[0]) u.createNestedArray(F("Body")).add(jsnip);
      }
    }
  }

  uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }
};