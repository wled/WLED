#include "wled.h"
/*
 * Register your v2 usermods here!
 */
#ifdef USERMOD_BH1750
#include "../usermods/BH1750_v2/usermod_BH1750.h"
#endif

void registerUsermods()
{
#ifdef USERMOD_BH1750
  usermods.add(new Usermod_BH1750());
#endif
}
