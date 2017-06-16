#pragma once

#include "envoy/init/init.h"

#include "common/json/json_validator.h"

namespace Envoy {
namespace Server {

/**
 * fixfix
 */
class LdsApi : Json::Validator, public Init::Target {
public:
  LdsApi(const Json::Object& config, Init::Manager& init_manager);

  // Init::Target
  void initialize(std::function<void()> callback) override;

private:
  std::function<void()> initialize_callback_;
};

} // Server
} // Envoy
