#pragma once

enum class TrainColor {
    Red,
    Orange,
    Yellow,
    Green,
    Blue,
    White,
    Unknown
};

inline const char * toString(TrainColor c) {
    switch (c) {
      case TrainColor::Red:    return "RED";
      case TrainColor::Orange: return "ORANGE";
      case TrainColor::Yellow: return "YELLOW";
      case TrainColor::Green:  return "GREEN";
      case TrainColor::Blue:   return "BLUE";
      case TrainColor::White:  return "WHITE";
    default:                   return "UNKNOWN";
    }
}

TrainColor parseTrainColor(const char * name);
