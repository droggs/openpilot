#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct CabanaSessionState {
  std::string recent_dbc_file;
  std::string active_msg_id;
  std::vector<std::string> selected_msg_ids;
  std::vector<std::string> active_charts;

};

struct CabanaPersistentState {
  std::string last_dir;
  std::vector<std::string> recent_files;
  CabanaSessionState session;
};

void rememberRecentFile(CabanaPersistentState &state, const std::string &filename, size_t max_recent_files);
