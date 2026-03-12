#include "tools/cabana/core/launch_config.h"

#include <sstream>

#include "tools/replay/replay.h"

namespace {

bool needsValue(size_t i, const std::vector<std::string> &args) {
  return i + 1 < args.size();
}

}  // namespace

CabanaLaunchParseResult parseCabanaLaunchConfig(const std::vector<std::string> &args) {
  CabanaLaunchParseResult result;

  for (size_t i = 1; i < args.size(); ++i) {
    const std::string &arg = args[i];

    if (arg == "-h" || arg == "--help") {
      result.show_help = true;
      return result;
    } else if (arg == "--demo") {
      result.config.demo = true;
    } else if (arg == "--auto") {
      result.config.auto_source = true;
    } else if (arg == "--qcam") {
      result.config.replay_flags |= REPLAY_FLAG_QCAMERA;
    } else if (arg == "--ecam") {
      result.config.replay_flags |= REPLAY_FLAG_ECAM;
    } else if (arg == "--dcam") {
      result.config.replay_flags |= REPLAY_FLAG_DCAM;
    } else if (arg == "--no-vipc") {
      result.config.replay_flags |= REPLAY_FLAG_NO_VIPC;
    } else if (arg == "--msgq") {
      result.config.mode = CabanaLaunchConfig::Mode::Msgq;
    } else if (arg == "--panda") {
      result.config.mode = CabanaLaunchConfig::Mode::Panda;
    } else if (arg == "--zmq") {
      if (!needsValue(i, args)) {
        result.ok = false;
        result.error = "--zmq requires an IP address";
        return result;
      }
      result.config.mode = CabanaLaunchConfig::Mode::Zmq;
      result.config.zmq_address = args[++i];
    } else if (arg == "--panda-serial") {
      if (!needsValue(i, args)) {
        result.ok = false;
        result.error = "--panda-serial requires a serial value";
        return result;
      }
      result.config.mode = CabanaLaunchConfig::Mode::Panda;
      result.config.panda_serial = args[++i];
    } else if (arg == "--socketcan") {
      if (!needsValue(i, args)) {
        result.ok = false;
        result.error = "--socketcan requires a device name";
        return result;
      }
      result.config.mode = CabanaLaunchConfig::Mode::SocketCan;
      result.config.socketcan_device = args[++i];
    } else if (arg == "--data_dir") {
      if (!needsValue(i, args)) {
        result.ok = false;
        result.error = "--data_dir requires a directory";
        return result;
      }
      result.config.data_dir = args[++i];
    } else if (arg == "--dbc") {
      if (!needsValue(i, args)) {
        result.ok = false;
        result.error = "--dbc requires a file path";
        return result;
      }
      result.config.dbc_file = args[++i];
    } else if (!arg.empty() && arg[0] == '-') {
      result.ok = false;
      result.error = "unknown option: " + arg;
      return result;
    } else if (result.config.route.empty()) {
      result.config.route = arg;
    } else {
      result.ok = false;
      result.error = "unexpected positional argument: " + arg;
      return result;
    }
  }

  if (result.config.route.empty() && result.config.demo) {
    result.config.route = DEMO_ROUTE;
  }

  return result;
}

std::string cabanaLaunchHelp(const std::string &program_name) {
  std::ostringstream out;
  out
      << "Usage: " << program_name << " [route] [options]\n"
      << "Cabana startup options:\n"
      << "  -h, --help              Show this help\n"
      << "  --demo                  Use the demo route\n"
      << "  --auto                  Auto load replay source\n"
      << "  --dbc FILE              Open a DBC file\n"
      << "  --data_dir DIR          Local directory with routes\n"
      << "  --qcam                  Load qcamera\n"
      << "  --ecam                  Load wide road camera\n"
      << "  --dcam                  Load driver camera\n"
      << "  --no-vipc               Do not output video\n"
      << "  --msgq                  Read CAN messages from msgq\n"
      << "  --zmq IP                Read CAN messages from zmq\n"
      << "  --panda                 Read CAN messages from panda\n"
      << "  --panda-serial SERIAL   Read CAN messages from panda with serial\n"
      << "  --socketcan DEVICE      Read CAN messages from SocketCAN\n";
  return out.str();
}
