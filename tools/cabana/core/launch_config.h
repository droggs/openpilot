#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct CabanaLaunchConfig {
  enum class Mode {
    Replay,
    Msgq,
    Zmq,
    Panda,
    SocketCan,
  };

  Mode mode = Mode::Replay;
  std::string route;
  std::string dbc_file;
  std::string data_dir;
  std::string zmq_address;
  std::string panda_serial;
  std::string socketcan_device;
  uint32_t replay_flags = 0;
  bool auto_source = false;
  bool demo = false;
};

struct CabanaLaunchParseResult {
  CabanaLaunchConfig config;
  bool ok = true;
  bool show_help = false;
  std::string error;
};

CabanaLaunchParseResult parseCabanaLaunchConfig(const std::vector<std::string> &args);
std::string cabanaLaunchHelp(const std::string &program_name);
