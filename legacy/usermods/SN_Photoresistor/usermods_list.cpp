#include "wled.h"
/*
 * Register your v2 usermods here!
 */
#ifdef USERMOD_SN_PHOTORESISTOR
#include "../usermods/SN_Photoresistor/usermod_sn_photoresistor.h"
#endif

void registerUsermods()
{
#ifdef USERMOD_SN_PHOTORESISTOR
  UsermodManager::add(new Usermod_SN_Photoresistor());
#endif
}