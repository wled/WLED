/*#include "wled.h"

// Hier registrieren wir in v15.3 KEINE usermods mehr, 
// halten aber die drei vom System verlangten Standard-Funktionen bereit:
void userSetup() {}
void userConnected() {}
void userLoop() {}
*/

#include "wled.h"
#include "usermod_v2_hsv_ticker.h" // Genau auf deinen Dateinamen angepasst

// Wird beim Systemstart aufgerufen
void userSetup() {
  // Registriert deinen Usermod sicher im WLED-System
  usermods.add(new UsermodHSVTicker());
}

// Diese beiden Funktionen müssen existieren, bleiben aber leer
void userConnected() {}
void userLoop() {}
