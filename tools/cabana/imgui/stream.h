#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cereal/messaging/messaging.h"
#include "tools/cabana/imgui/dbcmanager.h"
#include "tools/cabana/core/color.h"
#include "tools/replay/util.h"

struct CanData {
  void compute(const MessageId &msg_id, const uint8_t *dat, const int size, double current_sec,
               double playback_speed, const std::vector<uint8_t> &mask, double in_freq = 0);

  double ts = 0.;
  uint32_t count = 0;
  double freq = 0;
  std::vector<uint8_t> dat;
  std::vector<CabanaColor> colors;

  struct ByteLastChange {
    double ts = 0;
    int delta = 0;
    int same_delta_counter = 0;
    bool suppressed = false;
  };
  std::vector<ByteLastChange> last_changes;
  std::vector<std::array<uint32_t, 8>> bit_flip_counts;
  double last_freq_update_ts = 0;
};

struct CanEvent {
  uint8_t src;
  uint32_t address;
  uint64_t mono_time;
  uint8_t size;
  uint8_t dat[];
};

struct CompareCanEvent {
  constexpr bool operator()(const CanEvent *const e, uint64_t ts) const { return e->mono_time < ts; }
  constexpr bool operator()(uint64_t ts, const CanEvent *const e) const { return ts < e->mono_time; }
};

typedef std::unordered_map<MessageId, std::vector<const CanEvent *>> MessageEventsMap;
using CanEventIter = std::vector<const CanEvent *>::const_iterator;

class AbstractStream {
public:
  AbstractStream();
  virtual ~AbstractStream() { *alive_ = false; }
  virtual void start() = 0;
  virtual void registerCallbacks();
  virtual bool liveStreaming() const { return true; }
  virtual void seekTo(double ts) {}
  virtual std::string routeName() const = 0;
  virtual std::string carFingerprint() const { return ""; }
  virtual double beginDateTimeSecs() const { return 0; }
  virtual uint64_t beginMonoTime() const { return 0; }
  virtual double minSeconds() const { return 0; }
  virtual double maxSeconds() const { return 0; }
  virtual void setSpeed(float speed) {}
  virtual double getSpeed() { return 1; }
  virtual bool isPaused() const { return false; }
  virtual void pause(bool pause) {}
  void setTimeRange(const std::optional<std::pair<double, double>> &range);
  const std::optional<std::pair<double, double>> &timeRange() const { return time_range_; }

  // Called each frame from the main loop (replaces Qt timer/signal mechanism)
  virtual void pollUpdates();

  inline double currentSec() const { return current_sec_; }
  inline uint64_t toMonoTime(double sec) const { return beginMonoTime() + std::max(sec, 0.0) * 1e9; }
  inline double toSeconds(uint64_t mono_time) const { return std::max(0.0, (mono_time - beginMonoTime()) / 1e9); }

  inline const std::unordered_map<MessageId, CanData> &lastMessages() const { return last_msgs; }
  bool isMessageActive(const MessageId &id) const;
  inline const MessageEventsMap &eventsMap() const { return events_; }
  inline const std::vector<const CanEvent *> &allEvents() const { return all_events_; }
  const CanData &lastMessage(const MessageId &id) const;
  const std::vector<const CanEvent *> &events(const MessageId &id) const;
  std::pair<CanEventIter, CanEventIter> eventsInRange(const MessageId &id, std::optional<std::pair<double, double>> time_range) const;

  size_t suppressHighlighted();
  void clearSuppressed();
  void suppressDefinedSignals(bool suppress);

  // Callback registration (replaces Qt signals)
  std::vector<std::function<void()>> on_paused;
  std::vector<std::function<void()>> on_resume;
  std::vector<std::function<void(double)>> on_seeking;
  std::vector<std::function<void(double)>> on_seeked_to;
  std::vector<std::function<void(const std::optional<std::pair<double, double>> &)>> on_time_range_changed;
  std::vector<std::function<void(const MessageEventsMap &)>> on_events_merged;
  std::vector<std::function<void(const std::set<MessageId> *, bool)>> on_msgs_received;
  std::vector<std::function<void(const SourceSet &)>> on_sources_updated;

  SourceSet sources;

protected:
  void mergeEvents(const std::vector<const CanEvent *> &events);
  const CanEvent *newEvent(uint64_t mono_time, const cereal::CanData::Reader &c);
  void updateEvent(const MessageId &id, double sec, const uint8_t *data, uint8_t size);
  void requestUpdateLastMsgs() { update_last_msgs_pending_.store(true, std::memory_order_release); }
  void updateLastMsgsTo(double sec);
  std::vector<const CanEvent *> all_events_;
  double current_sec_ = 0;
  std::optional<std::pair<double, double>> time_range_;

private:
  void updateLastMessages();
  void updateMasks();

  MessageEventsMap events_;
  std::unordered_map<MessageId, CanData> last_msgs;
  std::unique_ptr<MonotonicBuffer> event_buffer_;
  std::atomic<bool> update_last_msgs_pending_ = false;

  // Members accessed in multiple threads. (mutex protected)
  std::mutex mutex_;
  std::set<MessageId> new_msgs_;
  std::unordered_map<MessageId, CanData> messages_;
  std::unordered_map<MessageId, std::vector<uint8_t>> masks_;

protected:
  std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

class DummyStream : public AbstractStream {
public:
  DummyStream() : AbstractStream() {}
  std::string routeName() const override { return "No Stream"; }
  void start() override {}
};

// A global pointer referring to the unique AbstractStream object
extern AbstractStream *can;
