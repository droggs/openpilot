#pragma once

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <tuple>

struct MessageId {
  uint8_t source = 0;
  uint32_t address = 0;

  std::string toString() const {
    char buf[64];
    snprintf(buf, sizeof(buf), "%u:%X", source, address);
    return buf;
  }

  inline static MessageId fromString(const std::string &str) {
    auto pos = str.find(':');
    if (pos == std::string::npos) return {};
    return MessageId{.source = uint8_t(std::stoul(str.substr(0, pos))),
                     .address = uint32_t(std::stoul(str.substr(pos + 1), nullptr, 16))};
  }

  bool operator==(const MessageId &other) const {
    return source == other.source && address == other.address;
  }

  bool operator!=(const MessageId &other) const {
    return !(*this == other);
  }

  bool operator<(const MessageId &other) const {
    return std::tie(source, address) < std::tie(other.source, other.address);
  }

  bool operator>(const MessageId &other) const {
    return std::tie(source, address) > std::tie(other.source, other.address);
  }
};

template <>
struct std::hash<MessageId> {
  std::size_t operator()(const MessageId &k) const noexcept {
    return std::hash<uint8_t>{}(k.source) ^ (std::hash<uint32_t>{}(k.address) << 1);
  }
};
