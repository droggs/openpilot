#include "tools/cabana/imgui/settings.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "third_party/json11/json11.hpp"

Settings settings;

static std::string settingsPath() {
  const char *home = std::getenv("HOME");
  return std::string(home ? home : "/tmp") + "/.cabana_settings.json";
}

// One-time migration from Qt QSettings ("cabana") INI format to our JSON format.
// QSettings("cabana") on Linux writes to ~/.config/cabana.conf in INI format.
static void migrateFromQtSettings(Settings &s) {
  const char *home = std::getenv("HOME");
  if (!home) return;
  std::string qt_path = std::string(home) + "/.config/cabana.conf";
  if (!std::filesystem::exists(qt_path)) return;

  std::ifstream f(qt_path);
  if (!f.is_open()) return;

  // Simple INI parser for QSettings format: "key=value" lines, ignoring sections like [General]
  std::string line;
  while (std::getline(f, line)) {
    // Remove trailing \r
    if (!line.empty() && line.back() == '\r') line.pop_back();
    // Skip section headers and empty lines
    if (line.empty() || line[0] == '[' || line[0] == '#') continue;
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string key = line.substr(0, eq);
    std::string val = line.substr(eq + 1);

    // Helper to split QSettings string list values (QStringList uses \x1f separator in INI)
    auto splitQtList = [](const std::string &v) -> std::vector<std::string> {
      std::vector<std::string> result;
      std::string item;
      for (char c : v) {
        if (c == '\x1f') {
          if (!item.empty()) result.push_back(item);
          item.clear();
        } else {
          item += c;
        }
      }
      if (!item.empty()) result.push_back(item);
      return result;
    };

    if (key == "theme" && !val.empty()) s.theme = std::stoi(val);
    else if (key == "fps" && !val.empty()) s.fps = std::stoi(val);
    else if (key == "max_cached_minutes" && !val.empty()) s.max_cached_minutes = std::stoi(val);
    else if (key == "chart_height" && !val.empty()) s.chart_height = std::stoi(val);
    else if (key == "chart_range" && !val.empty()) s.chart_range = std::stoi(val);
    else if (key == "chart_column_count" && !val.empty()) s.chart_column_count = std::stoi(val);
    else if (key == "chart_series_type" && !val.empty()) s.chart_series_type = std::stoi(val);
    else if (key == "last_dir" && !val.empty()) s.last_dir = val;
    else if (key == "last_route_dir" && !val.empty()) s.last_route_dir = val;
    else if (key == "log_path" && !val.empty()) s.log_path = val;
    else if (key == "drag_direction" && !val.empty()) s.drag_direction = static_cast<Settings::DragDirection>(std::stoi(val));
    else if (key == "multiple_lines_hex") s.multiple_lines_hex = (val == "true");
    else if (key == "log_livestream") s.log_livestream = (val == "true");
    else if (key == "suppress_defined_signals") s.suppress_defined_signals = (val == "true");
    else if (key == "absolute_time") s.absolute_time = (val == "true");
    else if (key == "sparkline_range" && !val.empty()) s.sparkline_range = std::stoi(val);
    else if (key == "recent_dbc_file" && !val.empty()) s.recent_dbc_file = val;
    else if (key == "active_msg_id" && !val.empty()) s.active_msg_id = val;
    else if (key == "recent_files" && !val.empty()) s.recent_files = splitQtList(val);
    else if (key == "selected_msg_ids" && !val.empty()) s.selected_msg_ids = splitQtList(val);
    else if (key == "active_charts" && !val.empty()) s.active_charts = splitQtList(val);
  }
}

Settings::Settings() {
  const char *home = std::getenv("HOME");
  std::string home_dir = home ? home : "/tmp";
  last_dir = home_dir;
  last_route_dir = home_dir;
  log_path = home_dir + "/cabana_live_stream/";
  load();
  // Re-import from Qt QSettings whenever the Qt file is newer than our JSON file,
  // keeping settings in sync while both apps coexist during migration.
  std::string json_path = settingsPath();
  std::string qt_path = home_dir + "/.config/cabana.conf";
  if (std::filesystem::exists(qt_path)) {
    if (!std::filesystem::exists(json_path) ||
        std::filesystem::last_write_time(qt_path) > std::filesystem::last_write_time(json_path)) {
      migrateFromQtSettings(*this);
    }
  }
}

Settings::~Settings() {
  save();
}

void Settings::load() {
  std::ifstream f(settingsPath());
  if (!f.is_open()) return;

  std::stringstream buf;
  buf << f.rdbuf();
  std::string err;
  auto j = json11::Json::parse(buf.str(), err);
  if (!err.empty()) return;

  if (j["absolute_time"].is_bool()) absolute_time = j["absolute_time"].bool_value();
  if (j["fps"].is_number()) fps = j["fps"].int_value();
  if (j["max_cached_minutes"].is_number()) max_cached_minutes = j["max_cached_minutes"].int_value();
  if (j["chart_height"].is_number()) chart_height = j["chart_height"].int_value();
  if (j["chart_range"].is_number()) chart_range = j["chart_range"].int_value();
  if (j["chart_column_count"].is_number()) chart_column_count = j["chart_column_count"].int_value();
  if (j["chart_series_type"].is_number()) chart_series_type = j["chart_series_type"].int_value();
  if (j["theme"].is_number()) theme = j["theme"].int_value();
  if (j["sparkline_range"].is_number()) sparkline_range = j["sparkline_range"].int_value();
  if (j["multiple_lines_hex"].is_bool()) multiple_lines_hex = j["multiple_lines_hex"].bool_value();
  if (j["log_livestream"].is_bool()) log_livestream = j["log_livestream"].bool_value();
  if (j["suppress_defined_signals"].is_bool()) suppress_defined_signals = j["suppress_defined_signals"].bool_value();
  if (j["log_path"].is_string()) log_path = j["log_path"].string_value();
  if (j["last_dir"].is_string()) last_dir = j["last_dir"].string_value();
  if (j["last_route_dir"].is_string()) last_route_dir = j["last_route_dir"].string_value();
  if (j["drag_direction"].is_number()) drag_direction = static_cast<DragDirection>(j["drag_direction"].int_value());
  if (j["window_x"].is_number()) window_x = j["window_x"].int_value();
  if (j["window_y"].is_number()) window_y = j["window_y"].int_value();
  if (j["window_width"].is_number()) window_width = j["window_width"].int_value();
  if (j["window_height"].is_number()) window_height = j["window_height"].int_value();
  if (j["window_maximized"].is_bool()) window_maximized = j["window_maximized"].bool_value();
  if (j["recent_dbc_file"].is_string()) recent_dbc_file = j["recent_dbc_file"].string_value();
  if (j["active_msg_id"].is_string()) active_msg_id = j["active_msg_id"].string_value();

  if (j["recent_files"].is_array()) {
    recent_files.clear();
    for (const auto &item : j["recent_files"].array_items())
      if (item.is_string()) recent_files.push_back(item.string_value());
  }
  if (j["selected_msg_ids"].is_array()) {
    selected_msg_ids.clear();
    for (const auto &item : j["selected_msg_ids"].array_items())
      if (item.is_string()) selected_msg_ids.push_back(item.string_value());
  }
  if (j["active_charts"].is_array()) {
    active_charts.clear();
    for (const auto &item : j["active_charts"].array_items())
      if (item.is_string()) active_charts.push_back(item.string_value());
  }
}

void Settings::save() {
  json11::Json::array rf_arr, sm_arr, ac_arr;
  for (const auto &s : recent_files) rf_arr.push_back(s);
  for (const auto &s : selected_msg_ids) sm_arr.push_back(s);
  for (const auto &s : active_charts) ac_arr.push_back(s);

  json11::Json j = json11::Json::object {
    {"absolute_time", absolute_time},
    {"fps", fps},
    {"max_cached_minutes", max_cached_minutes},
    {"chart_height", chart_height},
    {"chart_range", chart_range},
    {"chart_column_count", chart_column_count},
    {"chart_series_type", chart_series_type},
    {"theme", theme},
    {"sparkline_range", sparkline_range},
    {"multiple_lines_hex", multiple_lines_hex},
    {"log_livestream", log_livestream},
    {"suppress_defined_signals", suppress_defined_signals},
    {"log_path", log_path},
    {"last_dir", last_dir},
    {"last_route_dir", last_route_dir},
    {"drag_direction", static_cast<int>(drag_direction)},
    {"window_x", window_x},
    {"window_y", window_y},
    {"window_width", window_width},
    {"window_height", window_height},
    {"window_maximized", window_maximized},
    {"recent_dbc_file", recent_dbc_file},
    {"active_msg_id", active_msg_id},
    {"recent_files", rf_arr},
    {"selected_msg_ids", sm_arr},
    {"active_charts", ac_arr},
  };

  std::ofstream f(settingsPath());
  if (f.is_open()) {
    f << j.dump();
  }
}

void Settings::emitChanged() {
  for (auto &cb : on_changed_) {
    cb();
  }
}
