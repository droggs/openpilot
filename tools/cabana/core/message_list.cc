#include "tools/cabana/core/message_list.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <limits>
#include <tuple>

namespace {

constexpr uint8_t kInvalidSource = 0xff;

std::string toLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
  return s;
}

bool containsCaseInsensitive(const std::string &haystack, const std::string &needle) {
  return toLower(haystack).find(toLower(needle)) != std::string::npos;
}

bool parseRange(const std::string &filter, uint32_t value, int base = 10) {
  unsigned int min = std::numeric_limits<unsigned int>::min();
  unsigned int max = std::numeric_limits<unsigned int>::max();

  size_t dash = filter.find('-');
  bool ok = dash == std::string::npos || filter.find('-', dash + 1) == std::string::npos;
  if (!ok) return false;

  std::string first = dash == std::string::npos ? filter : filter.substr(0, dash);
  std::string second = dash == std::string::npos ? std::string() : filter.substr(dash + 1);

  try {
    if (!first.empty()) min = std::stoul(first, nullptr, base);
    if (dash == std::string::npos) {
      max = min;
    } else if (!second.empty()) {
      max = std::stoul(second, nullptr, base);
    }
  } catch (...) {
    return false;
  }

  return value >= min && value <= max;
}

std::string hexAddress(uint32_t value) {
  char buf[32];
  snprintf(buf, sizeof(buf), "0x%02X", value);
  return buf;
}

bool matchesFilters(const MessageListItem &item, const MessageListFilter &filter) {
  if (!filter.show_inactive_messages && !item.active) return false;

  for (const auto &[column, text] : filter.filters) {
    bool match = true;
    switch (column) {
      case 0:
        match = containsCaseInsensitive(item.name, text);
        if (!match) {
          match = std::any_of(item.signal_names.begin(), item.signal_names.end(),
                              [&](const auto &signal_name) { return containsCaseInsensitive(signal_name, text); });
        }
        break;
      case 1:
        match = parseRange(text, item.id.source);
        break;
      case 2:
        match = containsCaseInsensitive(hexAddress(item.id.address), text) || parseRange(text, item.id.address, 16);
        break;
      case 3:
        match = containsCaseInsensitive(item.node, text);
        break;
      case 4:
        match = parseRange(text, item.freq);
        break;
      case 5:
        match = parseRange(text, item.count);
        break;
      case 6:
        match = containsCaseInsensitive(item.data_hex, text);
        break;
      default:
        break;
    }
    if (!match) return false;
  }

  return true;
}

void sortItems(std::vector<MessageListItem> &items, const MessageListFilter &filter) {
  auto compare = [&](const auto &l, const auto &r) {
    switch (filter.sort_column) {
      case 0: return std::tie(l.name, l.id) < std::tie(r.name, r.id);
      case 1: return std::tie(l.id.source, l.id.address) < std::tie(r.id.source, r.id.address);
      case 2: return std::tie(l.id.address, l.id.source) < std::tie(r.id.address, r.id.source);
      case 3: return std::tie(l.node, l.id) < std::tie(r.node, r.id);
      case 4: return std::tie(l.freq, l.id) < std::tie(r.freq, r.id);
      case 5: return std::tie(l.count, l.id) < std::tie(r.count, r.id);
      default: return false;
    }
  };

  if (filter.descending) {
    std::stable_sort(items.rbegin(), items.rend(), compare);
  } else {
    std::stable_sort(items.begin(), items.end(), compare);
  }
}

}  // namespace

std::vector<MessageId> mergeMessageIds(const std::vector<MessageId> &can_ids, const std::set<MessageId> &dbc_ids) {
  std::vector<MessageId> all_messages;
  all_messages.reserve(can_ids.size() + dbc_ids.size());

  auto remaining_dbc = dbc_ids;
  for (const auto &id : can_ids) {
    all_messages.push_back(id);
    remaining_dbc.erase(MessageId{.source = kInvalidSource, .address = id.address});
  }
  all_messages.insert(all_messages.end(), remaining_dbc.begin(), remaining_dbc.end());
  return all_messages;
}

std::vector<MessageListItem> filterAndSortMessageList(const std::vector<MessageListItem> &items, const MessageListFilter &filter) {
  std::vector<MessageListItem> filtered;
  filtered.reserve(items.size());

  for (const auto &item : items) {
    if (matchesFilters(item, filter)) {
      filtered.push_back(item);
    }
  }

  sortItems(filtered, filter);
  return filtered;
}
