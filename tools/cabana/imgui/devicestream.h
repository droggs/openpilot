#pragma once

#include <string>

#include "tools/cabana/imgui/livestream.h"

class DeviceStream : public LiveStream {
public:
  DeviceStream(std::string address = {});
  ~DeviceStream();
  inline std::string routeName() const override {
    return "Live Streaming From " + (zmq_address.empty() ? std::string("127.0.0.1") : zmq_address);
  }
  const std::string &lastError() const { return last_error_; }
  void start() override;

protected:
  void streamThread() override;
  pid_t bridge_pid = -1;
  bool started_ = false;
  const std::string zmq_address;
  std::string last_error_;
};
