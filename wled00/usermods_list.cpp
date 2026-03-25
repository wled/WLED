#include "wled.h"

/*
 * Conditionally include and register usermods that are enabled via build
 * flags (-D USERMOD_xxx).  Each included header is expected to define its
 * Usermod-derived class, a static instance, and call REGISTER_USERMOD().
 */

#ifdef USERMOD_ZIGBEE_RGB_LIGHT
  #include "../usermods/zigbee_rgb_light/usermod_zigbee_rgb_light.h"
#endif
