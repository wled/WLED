#ifndef WEB_UI_MANAGER_H
#define WEB_UI_MANAGER_H

#include "wled.h"


class WebUIManager {
  public:
    WebUIManager() {}
    void registerEndpoints();
};

#endif // WEB_UI_MANAGER_H