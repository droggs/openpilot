#pragma once

#include <cmath>
#include <string>
#include <vector>

#include "tools/cabana/dbc/dbc.h"

bool cabanaShutdownRequested();
void clearCabanaShutdownRequested();
int num_decimals(double num);
std::string signalToolTip(const cabana::Signal *sig);
void initApp(int argc, char *argv[]);

namespace utils {

std::string formatSeconds(double sec, bool include_milliseconds = false, bool absolute_time = false);

inline std::string toHex(const std::vector<uint8_t> &dat, char separator = '\0') {
  static const char hex_chars[] = "0123456789ABCDEF";
  std::string result;
  result.reserve(dat.size() * (separator ? 3 : 2));
  for (size_t i = 0; i < dat.size(); ++i) {
    if (separator && i > 0) result += separator;
    result += hex_chars[(dat[i] >> 4) & 0xF];
    result += hex_chars[dat[i] & 0xF];
  }
  return result;
}

}  // namespace utils
