#pragma once

#include <ctime>
#include <memory>
#include <string>
#include "wled.h"

/// Config interface
struct IConfigurable {
  virtual ~IConfigurable() = default;
  virtual void addToConfig(JsonObject& root) = 0;
  virtual void appendConfigData(Print& s) {}
  virtual bool readFromConfig(JsonObject& root,
                              bool startup_complete,
                              bool& invalidate_history) = 0;
  virtual const char* configKey() const = 0;
};

/// Templated data source interface
/// @tparam ModelType  The concrete data model type
template<typename ModelType>
class IDataSourceT : public IConfigurable {
public:
  virtual ~IDataSourceT() = default;

  /// Fetch new data, nullptr if no new data
  virtual std::unique_ptr<ModelType> fetch(std::time_t now) = 0;

  /// Backfill older history if needed, nullptr if no new data
  virtual std::unique_ptr<ModelType> checkhistory(std::time_t now, std::time_t oldestTstamp) = 0;

  /// Force the internal schedule to fetch ASAP (e.g. after ON or re-enable)
  virtual void reload(std::time_t now) = 0;

  /// Identify the source (optional)
  virtual std::string name() const = 0;
};

/// Templated data view interface
/// @tparam ModelType  The concrete data model type
template<typename ModelType>
class IDataViewT : public IConfigurable {
public:
  virtual ~IDataViewT() = default;

  /// Render the model to output (LEDs, serial, etc.)
  virtual void view(std::time_t now, const ModelType& model, int16_t dbgPixelIndex) = 0;

  /// Identify the view (optional)
  virtual std::string name() const = 0;

  /// Append DebugPixel info
  virtual void appendDebugPixel(Print& s) const = 0;

  /// Allow view to release any persistent resources (e.g. segment freeze)
  /// when the usermod is inactive.
  virtual void deactivate() {}
};
