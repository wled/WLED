#include <cstring>

#include "train_color.h"

TrainColor parseTrainColor(const char * name) {
  if      (strcmp(name, "RED")==0)    return TrainColor::Red;
  else if (strcmp(name, "ORANGE")==0) return TrainColor::Orange;
  else if (strcmp(name, "YELLOW")==0) return TrainColor::Yellow;
  else if (strcmp(name, "GREEN")==0)  return TrainColor::Green;
  else if (strcmp(name, "BLUE")==0)   return TrainColor::Blue;
  else if (strcmp(name, "WHITE")==0)  return TrainColor::White;
  return TrainColor::Unknown;
}
