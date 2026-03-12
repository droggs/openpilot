#include "tools/cabana/imgui/bootstrap.h"

#include "tools/cabana/imgui/devicestream.h"
#include "tools/cabana/imgui/pandastream.h"
#include "tools/cabana/imgui/replaystream.h"
#ifdef __linux__
#include "tools/cabana/imgui/socketcanstream.h"
#endif

AbstractStream *createStreamForLaunchConfig(const CabanaLaunchConfig &config, std::string *error) {
  if (config.mode == CabanaLaunchConfig::Mode::Msgq) {
    return new DeviceStream();
  }
  if (config.mode == CabanaLaunchConfig::Mode::Zmq) {
    return new DeviceStream(config.zmq_address);
  }
  if (config.mode == CabanaLaunchConfig::Mode::Panda) {
    return new PandaStream({.serial = config.panda_serial});
  }
#ifdef __linux__
  if (config.mode == CabanaLaunchConfig::Mode::SocketCan) {
    if (!SocketCanStream::available()) {
      if (error) *error = "SocketCAN is not available on this system.";
      return nullptr;
    }
    return new SocketCanStream({.device = config.socketcan_device});
  }
#endif

  if (config.route.empty()) return nullptr;

  auto replay_stream = std::make_unique<ReplayStream>();
  if (!replay_stream->loadRoute(config.route, config.data_dir, config.replay_flags, config.auto_source)) {
    if (error) {
      RouteLoadError err = replay_stream->getReplay()->lastRouteError();
      if (err == RouteLoadError::Unauthorized) {
        *error = "Failed to load route '" + config.route + "': Authentication required. Set your comma JWT token.";
      } else if (err == RouteLoadError::AccessDenied) {
        *error = "Failed to load route '" + config.route + "': Access denied.";
      } else if (err == RouteLoadError::NetworkError) {
        *error = "Failed to load route '" + config.route + "': Network error. Check your connection.";
      } else if (err == RouteLoadError::FileNotFound) {
        *error = "Failed to load route '" + config.route + "': Route not found.";
      } else {
        *error = "Failed to load route '" + config.route + "'";
      }
    }
    return nullptr;
  }
  return replay_stream.release();
}
