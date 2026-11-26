#include "usermod_fpp.h"
#include "usermod_fseq.h"
#include "wled.h"

#ifdef USERMOD_FSEQ
UsermodFseq usermodFseq;
REGISTER_USERMOD(usermodFseq);
#endif

#ifdef USERMOD_FPP
UsermodFPP usermodFpp;
REGISTER_USERMOD(usermodFpp);
#endif
