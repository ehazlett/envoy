#pragma once

#include "envoy/common/pure.h"

namespace Envoy {
namespace Network {

class DrainDecision {
public:
  virtual ~DrainDecision() {}

  /**
   * @return TRUE if a connection should be drained and closed. It is up to individual network
   *         filters to determine when this should be called for the least impact possible.
   */
  virtual bool drainClose() PURE;
};

} // Network
} // Envoy
