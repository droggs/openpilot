#include "tools/cabana/imgui/dbcfile.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

// Helper: trim whitespace
static std::string trim(const std::string &s) {
  auto start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  auto end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

// Helper: check if string starts with prefix
static bool startsWith(const std::string &s, const std::string &prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

// Helper: check if string ends with suffix
static bool endsWith(const std::string &s, const std::string &suffix) {
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Helper: get basename without extension
static std::string baseName(const std::string &path) {
  auto p = std::filesystem::path(path);
  return p.stem().string();
}

// Helper: replace all occurrences
static std::string replaceAll(const std::string &s, const std::string &from, const std::string &to) {
  std::string result = s;
  size_t pos = 0;
  while ((pos = result.find(from, pos)) != std::string::npos) {
    result.replace(pos, from.size(), to);
    pos += to.size();
  }
  return result;
}

DBCFile::DBCFile(const std::string &dbc_file_name) {
  std::ifstream file(dbc_file_name);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file.");
  }
  name_ = baseName(dbc_file_name);
  filename = dbc_file_name;
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  parse(content);
}

DBCFile::DBCFile(const std::string &name, const std::string &content) : name_(name), filename("") {
  parse(content);
}

bool DBCFile::save() {
  assert(!filename.empty());
  return writeContents(filename);
}

bool DBCFile::saveAs(const std::string &new_filename) {
  filename = new_filename;
  return save();
}

bool DBCFile::writeContents(const std::string &fn) {
  std::ofstream file(fn);
  if (!file.is_open()) return false;
  std::string content = generateDBC();
  file.write(content.c_str(), content.size());
  return file.good();
}

void DBCFile::updateMsg(const MessageId &id, const std::string &name, uint32_t size, const std::string &node, const std::string &comment) {
  auto &m = msgs[id.address];
  m.address = id.address;
  m.name = name;
  m.size = size;
  m.transmitter = node.empty() ? DEFAULT_NODE_NAME : node;
  m.comment = comment;
}

cabana::Msg *DBCFile::msg(uint32_t address) {
  auto it = msgs.find(address);
  return it != msgs.end() ? &it->second : nullptr;
}

cabana::Msg *DBCFile::msg(const std::string &name) {
  auto it = std::find_if(msgs.begin(), msgs.end(), [&name](auto &m) { return m.second.name == name; });
  return it != msgs.end() ? &(it->second) : nullptr;
}

cabana::Signal *DBCFile::signal(uint32_t address, const std::string &name) {
  auto m = msg(address);
  return m ? (cabana::Signal *)m->sig(name) : nullptr;
}

void DBCFile::parse(const std::string &content) {
  msgs.clear();
  footer.clear();

  int line_num = 0;
  cabana::Msg *current_msg = nullptr;
  int multiplexor_cnt = 0;
  bool seen_first = false;

  // We need to handle multi-line comments, so we'll also keep the full content
  // First, do a line-by-line pass for BO_, SG_, and single-line CM_/VAL_
  std::istringstream stream(content);
  std::string raw_line;

  while (std::getline(stream, raw_line)) {
    ++line_num;
    // Remove trailing \r if present
    if (!raw_line.empty() && raw_line.back() == '\r') {
      raw_line.pop_back();
    }
    std::string line = trim(raw_line);

    bool seen = true;
    try {
      if (startsWith(line, "BO_ ")) {
        multiplexor_cnt = 0;
        current_msg = parseBO(line);
      } else if (startsWith(line, "SG_ ")) {
        parseSG(line, current_msg, multiplexor_cnt);
      } else if (startsWith(line, "VAL_ ")) {
        parseVAL(line);
      } else if (startsWith(line, "CM_ BO_")) {
        // For multi-line comments, accumulate until we find ";
        std::string cm_line = line;
        while (!endsWith(cm_line, "\";") && !stream.eof()) {
          std::string next_line;
          std::getline(stream, next_line);
          if (!next_line.empty() && next_line.back() == '\r') next_line.pop_back();
          cm_line += "\n" + next_line;
          ++line_num;
        }
        parseCM_BO(cm_line);
      } else if (startsWith(line, "CM_ SG_ ")) {
        std::string cm_line = line;
        while (!endsWith(cm_line, "\";") && !stream.eof()) {
          std::string next_line;
          std::getline(stream, next_line);
          if (!next_line.empty() && next_line.back() == '\r') next_line.pop_back();
          cm_line += "\n" + next_line;
          ++line_num;
        }
        parseCM_SG(cm_line);
      } else {
        seen = false;
      }
    } catch (std::exception &e) {
      char buf[512];
      snprintf(buf, sizeof(buf), "[%s:%d]%s: %s", filename.c_str(), line_num, e.what(), line.c_str());
      throw std::runtime_error(buf);
    }

    if (seen) {
      seen_first = true;
    } else if (!seen_first) {
      header += raw_line + "\n";
    } else {
      // Preserve unrecognized lines after first recognized section (BA_, BA_DEF_, BU_, etc.)
      footer += raw_line + "\n";
    }
  }

  for (auto &[_, m] : msgs) {
    m.update();
  }
}

cabana::Msg *DBCFile::parseBO(const std::string &line) {
  static std::regex bo_regexp(R"(^BO_ (\w+) (\w+) *: (\w+) (\w+))");

  std::smatch match;
  if (!std::regex_search(line, match, bo_regexp))
    throw std::runtime_error("Invalid BO_ line format");

  uint32_t address = std::stoul(match[1].str());
  if (msgs.count(address) > 0) {
    throw std::runtime_error("Duplicate message address: " + std::to_string(address));
  }

  cabana::Msg *msg = &msgs[address];
  msg->address = address;
  msg->name = match[2].str();
  msg->size = std::stoul(match[3].str());
  msg->transmitter = trim(match[4].str());
  return msg;
}

void DBCFile::parseSG(const std::string &line, cabana::Msg *current_msg, int &multiplexor_cnt) {
  static std::regex sg_regexp(R"(^SG_ (\w+) *: (\d+)\|(\d+)@(\d+)([\+\-]) \(([0-9.+\-eE]+),([0-9.+\-eE]+)\) \[([0-9.+\-eE]+)\|([0-9.+\-eE]+)\] \"(.*)\" (.*))");
  static std::regex sgm_regexp(R"(^SG_ (\w+) (\w+) *: (\d+)\|(\d+)@(\d+)([\+\-]) \(([0-9.+\-eE]+),([0-9.+\-eE]+)\) \[([0-9.+\-eE]+)\|([0-9.+\-eE]+)\] \"(.*)\" (.*))");

  if (!current_msg)
    throw std::runtime_error("No Message");

  int offset = 0;
  std::smatch match;
  if (!std::regex_search(line, match, sg_regexp)) {
    if (!std::regex_search(line, match, sgm_regexp)) {
      throw std::runtime_error("Invalid SG_ line format");
    }
    offset = 1;
  }

  std::string name = match[1].str();
  if (current_msg->sig(name) != nullptr)
    throw std::runtime_error("Duplicate signal name");

  cabana::Signal s{};
  if (offset == 1) {
    auto indicator = match[2].str();
    if (indicator == "M") {
      ++multiplexor_cnt;
      if (multiplexor_cnt >= 2)
        throw std::runtime_error("Multiple multiplexor");
      s.type = cabana::Signal::Type::Multiplexor;
    } else {
      s.type = cabana::Signal::Type::Multiplexed;
      s.multiplex_value = std::stoi(indicator.substr(1));
    }
  }
  s.name = name;
  s.start_bit = std::stoi(match[offset + 2].str());
  s.size = std::stoi(match[offset + 3].str());
  s.is_little_endian = std::stoi(match[offset + 4].str()) == 1;
  s.is_signed = match[offset + 5].str() == "-";
  s.factor = std::stod(match[offset + 6].str());
  s.offset = std::stod(match[offset + 7].str());
  s.min = std::stod(match[offset + 8].str());
  s.max = std::stod(match[offset + 9].str());
  s.unit = match[offset + 10].str();
  s.receiver_name = trim(match[offset + 11].str());
  current_msg->sigs.push_back(new cabana::Signal(s));
}

void DBCFile::parseCM_BO(const std::string &line) {
  static std::regex msg_comment_regexp(R"(CM_ BO_ *(\w+) *\"((?:[^"\\]|\\.)*)\"\s*;)");

  std::smatch match;
  if (!std::regex_search(line, match, msg_comment_regexp))
    throw std::runtime_error("Invalid message comment format");

  if (auto m = msg(std::stoul(match[1].str())))
    m->comment = trim(replaceAll(match[2].str(), "\\\"", "\""));
}

void DBCFile::parseCM_SG(const std::string &line) {
  static std::regex sg_comment_regexp(R"(CM_ SG_ *(\w+) *(\w+) *\"((?:[^"\\]|\\.)*)\"\s*;)");

  std::smatch match;
  if (!std::regex_search(line, match, sg_comment_regexp))
    throw std::runtime_error("Invalid CM_ SG_ line format");

  if (auto s = signal(std::stoul(match[1].str()), match[2].str())) {
    s->comment = trim(replaceAll(match[3].str(), "\\\"", "\""));
  }
}

void DBCFile::parseVAL(const std::string &line) {
  static std::regex val_regexp(R"(VAL_ (\w+) (\w+) (\s*[-+]?[0-9]+\s+\".+?\"[^;]*))");

  std::smatch match;
  if (!std::regex_search(line, match, val_regexp))
    throw std::runtime_error("invalid VAL_ line format");

  if (auto s = signal(std::stoul(match[1].str()), match[2].str())) {
    // Split by quotes to get value-description pairs
    std::string desc_str = trim(match[3].str());
    // Parse pairs: number "description" number "description" ...
    size_t pos = 0;
    while (pos < desc_str.size()) {
      // Skip whitespace
      while (pos < desc_str.size() && std::isspace(desc_str[pos])) ++pos;
      if (pos >= desc_str.size()) break;

      // Read number
      size_t num_start = pos;
      while (pos < desc_str.size() && !std::isspace(desc_str[pos]) && desc_str[pos] != '"') ++pos;
      std::string num_str = trim(desc_str.substr(num_start, pos - num_start));
      if (num_str.empty()) break;

      // Skip whitespace
      while (pos < desc_str.size() && std::isspace(desc_str[pos])) ++pos;
      if (pos >= desc_str.size() || desc_str[pos] != '"') break;
      ++pos; // skip opening quote

      // Read description until closing quote
      size_t desc_start = pos;
      while (pos < desc_str.size() && desc_str[pos] != '"') ++pos;
      std::string desc = desc_str.substr(desc_start, pos - desc_start);
      if (pos < desc_str.size()) ++pos; // skip closing quote

      s->val_desc.push_back({std::stod(num_str), trim(desc)});
    }
  }
}

std::string DBCFile::generateDBC() {
  std::string dbc_string, comment, val_desc;
  for (const auto &[address, m] : msgs) {
    const std::string &transmitter = m.transmitter.empty() ? DEFAULT_NODE_NAME : m.transmitter;
    dbc_string += "BO_ " + std::to_string(address) + " " + m.name + ": " + std::to_string(m.size) + " " + transmitter + "\n";
    if (!m.comment.empty()) {
      std::string escaped_comment = replaceAll(m.comment, "\"", "\\\"");
      comment += "CM_ BO_ " + std::to_string(address) + " \"" + escaped_comment + "\";\n";
    }
    for (auto sig : m.getSignals()) {
      std::string multiplexer_indicator;
      if (sig->type == cabana::Signal::Type::Multiplexor) {
        multiplexer_indicator = "M ";
      } else if (sig->type == cabana::Signal::Type::Multiplexed) {
        multiplexer_indicator = "m" + std::to_string(sig->multiplex_value) + " ";
      }
      const std::string &recv = sig->receiver_name.empty() ? DEFAULT_NODE_NAME : sig->receiver_name;
      dbc_string += " SG_ " + sig->name + " " + multiplexer_indicator + ": " +
                    std::to_string(sig->start_bit) + "|" + std::to_string(sig->size) + "@" +
                    std::string(1, sig->is_little_endian ? '1' : '0') +
                    std::string(1, sig->is_signed ? '-' : '+') +
                    " (" + doubleToString(sig->factor) + "," + doubleToString(sig->offset) + ")" +
                    " [" + doubleToString(sig->min) + "|" + doubleToString(sig->max) + "]" +
                    " \"" + sig->unit + "\" " + recv + "\n";
      if (!sig->comment.empty()) {
        std::string escaped_comment = replaceAll(sig->comment, "\"", "\\\"");
        comment += "CM_ SG_ " + std::to_string(address) + " " + sig->name + " \"" + escaped_comment + "\";\n";
      }
      if (!sig->val_desc.empty()) {
        std::string text;
        for (auto &[val, desc] : sig->val_desc) {
          if (!text.empty()) text += " ";
          char val_buf[64];
          snprintf(val_buf, sizeof(val_buf), "%g", val);
          text += std::string(val_buf) + " \"" + desc + "\"";
        }
        val_desc += "VAL_ " + std::to_string(address) + " " + sig->name + " " + text + ";\n";
      }
    }
    dbc_string += "\n";
  }
  return header + dbc_string + comment + val_desc + footer;
}
