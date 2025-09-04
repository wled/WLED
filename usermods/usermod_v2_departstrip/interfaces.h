#pragma once

#include <ctime>
#include <memory>
#include <string>
#include "wled.h"

// Copied pattern from bartdepart/skystrip

struct IConfigurable {
  virtual ~IConfigurable() = default;
  virtual void addToConfig(JsonObject& root) = 0;
  virtual void appendConfigData(Print& s) {}
  virtual bool readFromConfig(JsonObject& root,
                              bool startup_complete,
                              bool& invalidate_history) = 0;
  virtual const char* configKey() const = 0;
};

template<typename ModelType>
class IDataSourceT : public IConfigurable {
public:
  virtual ~IDataSourceT() = default;
  virtual std::unique_ptr<ModelType> fetch(std::time_t now) = 0;
  virtual void reload(std::time_t now) = 0;
  virtual std::string name() const = 0;
};

template<typename ModelType>
class IDataViewT : public IConfigurable {
public:
  virtual ~IDataViewT() = default;
  virtual void view(std::time_t now, const ModelType& model) = 0;
  virtual std::string name() const = 0;
  virtual void appendConfigData(Print& s, const ModelType* model) {
    IConfigurable::appendConfigData(s);
  }
};
