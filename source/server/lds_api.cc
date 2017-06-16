#include "server/lds_api.h"

#include "common/json/config_schemas.h"

namespace Envoy {
namespace Server {

LdsApi::LdsApi(const Json::Object& config, Init::Manager& init_manager)
    : Json::Validator(config, Json::Schema::LDS_SCHEMA) {
  // fixfix check cluster, add basic test.
  init_manager.registerTarget(*this);
}

void LdsApi::initialize(std::function<void()> callback) {
  // TODO(mattklein123): Actually implement this in follow up PR.
  callback();
}

} // Server
} // Envoy
