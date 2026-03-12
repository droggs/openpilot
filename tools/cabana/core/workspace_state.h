#pragma once

#include <string>

struct CabanaWorkspaceState {
  bool has_stream = false;
  bool live_streaming = false;
  std::string route_name;
  std::string car_fingerprint;

  std::string videoPanelTitle() const;
};
