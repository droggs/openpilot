#include "tools/cabana/core/persistent_state.h"

#include <algorithm>

void rememberRecentFile(CabanaPersistentState &state, const std::string &filename, size_t max_recent_files) {
  state.recent_files.erase(std::remove(state.recent_files.begin(), state.recent_files.end(), filename), state.recent_files.end());
  state.recent_files.insert(state.recent_files.begin(), filename);
  if (state.recent_files.size() > max_recent_files) {
    state.recent_files.resize(max_recent_files);
  }
}
