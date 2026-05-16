#define USERMOD_HSV_TICKER
#pragma once
#include "wled.h"
#include <HTTPClient.h>

class UsermodHSVTicker : public Usermod {
  private:
    // Boot- und Netzwerk-Flags
    unsigned long bootTime = 0;
    bool bootSequenceTriggered = false;
    bool bootSequenceActive = false;
    bool bootSequenceDone = false;
    bool hasInternet = false;

    // Timer & Intervalle
    unsigned long lastApiCheck = 0;
    const unsigned long apiInterval = 30000; // Alle 30 Sekunden API-Abruf
    
    unsigned long presetTimerStart = 0;
    const unsigned long presetDuration = 60000; // 1 Minute (60.000 ms) für Tor-Effekt
    bool isTemporaryPresetActive = false;

    // Zustandstracker für Spielstände
    int lastOwnGoals = 0;
    int lastOpponentGoals = 0;
    bool wasPlayingLastCheck = false;

    // Filterbegriffe für die Vereine
    const String hsvFilter = "Hamburg"; 
    const String dfbFilter = "Deutschland"; 

    // Tracker, welches Team aktuell aktiv ist (1 = HSV, 2 = Deutschland)
    int activeTeamIndex = 0; 

    // Hilfsfunktion: Setzt alle LEDs temporär auf eine feste Farbe (für den Boot-Test)
    void setSolidColor(uint8_t r, uint8_t g, uint8_t b) {
      bri = 255; // Volle Helligkeit erzwingen
      uint16_t totalLeds = strip.getLengthTotal();
      for (uint16_t i = 0; i < totalLeds; i++) {
        strip.setPixelColor(i, RGBW32(r, g, b, 0));
      }
      strip.show();
    }

    // Verbindungstest mit Timeout-Absicherung gegen System-Freezes
    bool testInternetConnection() {
      if (!WLED_CONNECTED) return false;
      HTTPClient http;
      http.setTimeout(3000); // 3 Sekunden Timeout verhindert WLED-Absturz
      http.begin("https://openligadb.de"); 
      int httpCode = http.GET();
      http.end();
      return (httpCode == 200);
    }

    // JSON-Abfrage der jeweiligen Liga/Turnier-Schnittstelle
    void checkMatchData(const String& leagueShortcut, bool& isPlaying, int& ownGoals, int& opponentGoals, int& detectedTeam) {
      if (!WLED_CONNECTED || isPlaying) return; 

      HTTPClient http;
      http.setTimeout(4000); 
      String url = "https://openligadb.de" + leagueShortcut;
      http.begin(url);
      
      int httpCode = http.GET();
      if (httpCode == 200) {
        String payload = http.getString();
        
        // Nutzt WLEDs interne JSON-Klassen direkt
        ArduinoJson::DynamicJsonDocument doc(16384); 
        ArduinoJson::DeserializationError error = ArduinoJson::deserializeJson(doc, payload);
        
        if (error == ArduinoJson::DeserializationError::Ok) {
          ArduinoJson::JsonArray matches = doc.as<ArduinoJson::JsonArray>();
          for (ArduinoJson::JsonVariant m : matches) {
            ArduinoJson::JsonObject match = m.as<ArduinoJson::JsonObject>();
            String team1 = match["team1"]["teamName"].as<String>();
            String team2 = match["team2"]["teamName"].as<String>();
            bool isFinished = match["matchIsFinished"].as<bool>();
            
            bool matchFound = false;
            String activeFilter = "";
            int currentTeamType = 0;
            
            if (team1.indexOf(hsvFilter) >= 0 || team2.indexOf(hsvFilter) >= 0) {
              matchFound = true;
              activeFilter = hsvFilter;
              currentTeamType = 1; // HSV aktiv
            } else if (team1.indexOf(dfbFilter) >= 0 || team2.indexOf(dfbFilter) >= 0) {
              matchFound = true;
              activeFilter = dfbFilter;
              currentTeamType = 2; // Deutschland aktiv
            }

            if (!isFinished && matchFound) {
              isPlaying = true;
              detectedTeam = currentTeamType;
              int points1 = 0;
              int points2 = 0;
              
              ArduinoJson::JsonArray results = match["matchResults"].as<ArduinoJson::JsonArray>();
              if (results.size() > 0) {
                ArduinoJson::JsonVariant r = results[results.size() - 1];
                ArduinoJson::JsonObject latestResult = r.as<ArduinoJson::JsonObject>();
                points1 = latestResult["pointsTeam1"].as<int>();
                points2 = latestResult["pointsTeam2"].as<int>();
              }

              if (team1.indexOf(activeFilter) >= 0) {
                ownGoals = points1;
                opponentGoals = points2;
              } else {
                ownGoals = points2;
                opponentGoals = points1;
              }
              break; 
            }
          }
        }
      }
      http.end();
    }

  public:
    void setup() override {
      bootTime = millis();
    }

    void loop() override {
      unsigned long currentMillis = millis();

      // --- PHASE 1: BOOT-REAKTION (Start nach 30 Sekunden) ---
      if (!bootSequenceTriggered && (currentMillis - bootTime >= 30000)) {
        bootSequenceTriggered = true;
        bootSequenceActive = true;
        presetTimerStart = currentMillis; 
        hasInternet = testInternetConnection();
      }

      if (bootSequenceActive) {
        if (currentMillis - presetTimerStart < 30000) {
          if (hasInternet) {
            setSolidColor(0, 255, 0); // GRÜN
          } else {
            setSolidColor(255, 0, 0); // ROT
          }
          return; 
        } else {
          bootSequenceActive = false;
          bootSequenceDone = true;
          strip.getSegment(0).setColor(0, BLACK); 
          strip.show();
        }
      }

      if (!bootSequenceDone) return;


      // --- PHASE 2: REGULÄRER API-ABRUF (Alle 30 Sekunden) ---
      if (currentMillis - lastApiCheck >= apiInterval) {
        lastApiCheck = currentMillis;

        bool isCurrentlyPlaying = false;
        int currentOwnGoals = 0;
        int currentOpponentGoals = 0;
        int detectedTeam = 0; 

        // Prüfe nacheinander alle Wettbewerbe
        checkMatchData("bl1", isCurrentlyPlaying, currentOwnGoals, currentOpponentGoals, detectedTeam);
        if (!isCurrentlyPlaying) checkMatchData("bl2", isCurrentlyPlaying, currentOwnGoals, currentOpponentGoals, detectedTeam);
        if (!isCurrentlyPlaying) checkMatchData("dfb", isCurrentlyPlaying, currentOwnGoals, currentOpponentGoals, detectedTeam);
        if (!isCurrentlyPlaying) checkMatchData("nat", isCurrentlyPlaying, currentOwnGoals, currentOpponentGoals, detectedTeam);

        if (isCurrentlyPlaying) {
          activeTeamIndex = detectedTeam; 

          // Spiel startet frisch: Standard-Preset aktivieren
          if (!wasPlayingLastCheck) {
            if (activeTeamIndex == 1) applyPreset(90); // HSV Standard
            if (activeTeamIndex == 2) applyPreset(80); // Deutschland Standard
          }

          // TOR (HSV oder Deutschland) -> Preset für 1 Minute aktivieren
          if (wasPlayingLastCheck && (currentOwnGoals > lastOwnGoals)) {
            if (activeTeamIndex == 1) applyPreset(91); // HSV Tor
            if (activeTeamIndex == 2) applyPreset(81); // Deutschland Tor
            isTemporaryPresetActive = true;
            presetTimerStart = currentMillis;
          }

          // GEGENTOR (HSV oder Deutschland) -> Preset für 1 Minute aktivieren
          if (wasPlayingLastCheck && (currentOpponentGoals > lastOpponentGoals)) {
            if (activeTeamIndex == 1) applyPreset(92); // HSV Gegentor
            if (activeTeamIndex == 2) applyPreset(82); // Deutschland Gegentor
            isTemporaryPresetActive = true;
            presetTimerStart = currentMillis;
          }

          lastOwnGoals = currentOwnGoals;
          lastOpponentGoals = currentOpponentGoals;
        } 
        else if (wasPlayingLastCheck) {
          isTemporaryPresetActive = false;
          activeTeamIndex = 0;
        }

        wasPlayingLastCheck = isCurrentlyPlaying;
      }


      // --- PHASE 3: TIMER-RÜCKSPRUNG NACH TOREN ---
      if (isTemporaryPresetActive && (currentMillis - presetTimerStart >= presetDuration)) {
        isTemporaryPresetActive = false;
        if (activeTeamIndex == 1) applyPreset(90);
        if (activeTeamIndex == 2) applyPreset(80);
      }
    }
};
