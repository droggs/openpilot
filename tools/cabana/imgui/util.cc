#include "tools/cabana/imgui/util.h"

#include <atomic>
#include <chrono>
#include <climits>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string>

#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace {
std::atomic<bool> cabana_shutdown_requested_flag = false;
}

static void signal_handler(int s) {
  cabana_shutdown_requested_flag.store(true, std::memory_order_release);
  printf("\nexiting...\n");
}

bool cabanaShutdownRequested() {
  return cabana_shutdown_requested_flag.load(std::memory_order_acquire);
}

void clearCabanaShutdownRequested() {
  cabana_shutdown_requested_flag.store(false, std::memory_order_release);
}

int num_decimals(double num) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%g", num);
  const char *dot = strchr(buf, '.');
  return dot ? static_cast<int>(strlen(dot + 1)) : 0;
}

std::string signalToolTip(const cabana::Signal *sig) {
  char buf[512];
  snprintf(buf, sizeof(buf),
    "\n    %s\n"
    "    Start Bit: %d Size: %d\n"
    "    MSB: %d LSB: %d\n"
    "    Little Endian: %s Signed: %s\n",
    sig->name.c_str(), sig->start_bit, sig->size,
    sig->msb, sig->lsb,
    sig->is_little_endian ? "Y" : "N",
    sig->is_signed ? "Y" : "N");
  return buf;
}

void initApp(int argc, char *argv[]) {
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);
  std::signal(SIGPIPE, SIG_IGN);

  // Ensure the current dir matches the executable's directory
#ifdef __APPLE__
  char exe_path[PATH_MAX];
  uint32_t size = sizeof(exe_path);
  if (_NSGetExecutablePath(exe_path, &size) == 0) {
    std::string dir(exe_path);
    auto pos = dir.rfind('/');
    if (pos != std::string::npos) {
      (void)!chdir(dir.substr(0, pos).c_str());
    }
  }
#else
  char exe_path[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
  if (len > 0) {
    exe_path[len] = '\0';
    std::string dir(exe_path);
    auto pos = dir.rfind('/');
    if (pos != std::string::npos) {
      dir = dir.substr(0, pos);
      (void)!chdir(dir.c_str());
    }
  }
#endif
}

namespace utils {

std::string formatSeconds(double sec, bool include_milliseconds, bool absolute_time) {
  // Convert seconds to time components
  int64_t total_ms = static_cast<int64_t>(sec * 1000.0);
  int ms = std::abs(static_cast<int>(total_ms % 1000));

  if (absolute_time) {
    // Treat sec as epoch seconds, display in local time
    time_t t = static_cast<time_t>(sec);
    struct tm tm_buf;
    localtime_r(&t, &tm_buf);
    char buf[64];
    if (include_milliseconds) {
      strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
      char result[80];
      snprintf(result, sizeof(result), "%s.%03d", buf, ms);
      return result;
    } else {
      strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
      return buf;
    }
  }

  // Relative time formatting
  const char *sign = sec < 0 ? "-" : "";
  int64_t total_secs = static_cast<int64_t>(std::abs(sec));
  int hours = total_secs / 3600;
  int minutes = (total_secs % 3600) / 60;
  int secs = total_secs % 60;

  char buf[64];
  if (hours > 0) {
    if (include_milliseconds) {
      snprintf(buf, sizeof(buf), "%s%02d:%02d:%02d.%03d", sign, hours, minutes, secs, ms);
    } else {
      snprintf(buf, sizeof(buf), "%s%02d:%02d:%02d", sign, hours, minutes, secs);
    }
  } else {
    if (include_milliseconds) {
      snprintf(buf, sizeof(buf), "%s%02d:%02d.%03d", sign, minutes, secs, ms);
    } else {
      snprintf(buf, sizeof(buf), "%s%02d:%02d", sign, minutes, secs);
    }
  }
  return buf;
}

}  // namespace utils
