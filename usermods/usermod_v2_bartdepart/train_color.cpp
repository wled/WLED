#include <cstring>
#include <strings.h> // for strcasecmp

#include "train_color.h"

TrainColor parseTrainColor(const char * name) {
  if (!name) return TrainColor::Unknown;
  if      (strcasecmp(name, "RED")==0)    return TrainColor::Red;
  else if (strcasecmp(name, "ORANGE")==0) return TrainColor::Orange;
  else if (strcasecmp(name, "YELLOW")==0) return TrainColor::Yellow;
  else if (strcasecmp(name, "GREEN")==0)  return TrainColor::Green;
  else if (strcasecmp(name, "BLUE")==0)   return TrainColor::Blue;
  else if (strcasecmp(name, "WHITE")==0)  return TrainColor::White;
  return TrainColor::Unknown;
}
