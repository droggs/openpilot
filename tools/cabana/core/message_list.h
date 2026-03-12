#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "tools/cabana/core/message_id.h"

struct MessageListItem {
  MessageId id;
  std::string name;
  std::string node;
  std::vector<std::string> signal_names;
  std::string data_hex;
  double freq = 0.0;
  uint32_t count = 0;
  bool active = true;
};

struct MessageListFilter {
  std::map<int, std::string> filters;
  bool show_inactive_messages = true;
  int sort_column = 0;
  bool descending = false;
};

std::vector<MessageId> mergeMessageIds(const std::vector<MessageId> &can_ids, const std::set<MessageId> &dbc_ids);
std::vector<MessageListItem> filterAndSortMessageList(const std::vector<MessageListItem> &items, const MessageListFilter &filter);
