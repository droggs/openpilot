#include "tools/cabana/core/workspace_state.h"

std::string CabanaWorkspaceState::videoPanelTitle() const {
  if (car_fingerprint.empty() || live_streaming) {
    return route_name;
  }
  return "ROUTE: " + route_name + "  FINGERPRINT: " + car_fingerprint;
}
