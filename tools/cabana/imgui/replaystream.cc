#include "tools/cabana/imgui/replaystream.h"

#include <mutex>

#include "common/timing.h"
#include "common/util.h"
#include "tools/cabana/imgui/settings.h"

ReplayStream::ReplayStream() : AbstractStream() {
  unsetenv("ZMQ");
  setenv("COMMA_CACHE", "/tmp/comma_download_cache", 1);

#ifndef __APPLE__
  op_prefix = std::make_unique<OpenpilotPrefix>();
#endif
}

ReplayStream::~ReplayStream() {
  // Must destroy replay BEFORE member mutexes/cvs, since replay threads
  // reference them via callbacks. First unblock any thread waiting on merge_cv_.
  {
    std::lock_guard lk(merge_mutex_);
    merge_done_ = true;
  }
  merge_cv_.notify_all();
  // Clear callbacks to prevent use-after-free during replay thread shutdown
  if (replay) {
    replay->installEventFilter(nullptr);
    replay->onSeeking = nullptr;
    replay->onSeekedTo = nullptr;
    replay->onQLogLoaded = nullptr;
    replay->onSegmentsMerged = nullptr;
  }
  replay.reset();  // joins replay threads before members are destroyed
}

void ReplayStream::registerCallbacks() {
  AbstractStream::registerCallbacks();
  auto alive = alive_;
  settings.on_changed_.push_back([this, alive]() {
    if (*alive && replay) replay->setSegmentCacheLimit(settings.max_cached_minutes);
  });
}

void ReplayStream::mergeSegments() {
  auto event_data = replay->getEventData();
  for (const auto &[n, seg] : event_data->segments) {
    if (!processed_segments.count(n)) {
      processed_segments.insert(n);

      std::vector<const CanEvent *> new_events;
      new_events.reserve(seg->log->events.size());
      for (const Event &e : seg->log->events) {
        if (e.which == cereal::Event::Which::CAN) {
          capnp::FlatArrayMessageReader reader(e.data);
          auto event = reader.getRoot<cereal::Event>();
          for (const auto &c : event.getCan()) {
            new_events.push_back(newEvent(e.mono_time, c));
          }
        }
      }
      mergeEvents(new_events);
    }
  }
}

bool ReplayStream::loadRoute(const std::string &route, const std::string &data_dir, uint32_t replay_flags, bool auto_source) {
  replay.reset(new Replay(route, {"can", "roadEncodeIdx", "driverEncodeIdx", "wideRoadEncodeIdx", "carParams"},
                          {}, nullptr, replay_flags, data_dir, auto_source));
  replay->setSegmentCacheLimit(settings.max_cached_minutes);
  replay->installEventFilter([this](const Event *event) { return eventFilter(event); });

  replay->onSeeking = [this](double sec) {
    current_sec_ = sec;
    for (auto &cb : on_seeking) cb(sec);
  };
  replay->onSeekedTo = [this](double sec) {
    // Non-blocking: just store the target and set the flag.
    // The UI thread will process it in pollUpdates(). Multiple seeks between
    // frames are coalesced — only the latest target is used.
    {
      std::lock_guard lk(seek_mutex_);
      seek_target_sec_ = sec;
    }
    seek_pending_.store(true, std::memory_order_release);
  };
  // Forward qlogs loaded by Timeline (unfiltered, used for thumbnails)
  replay->onQLogLoaded = [this](std::shared_ptr<LogReader> qlog) {
    std::lock_guard lk(qlog_mutex_);
    pending_qlogs_.push_back(std::move(qlog));
  };
  // Replaces Qt::BlockingQueuedConnection: segment manager thread signals the main
  // thread to run mergeSegments(), then blocks until it's done.
  replay->onSegmentsMerged = [this]() {
    std::unique_lock lk(merge_mutex_);
    merge_done_ = false;
    merge_pending_.store(true, std::memory_order_release);
    merge_cv_.wait(lk, [this]() { return merge_done_; });
  };

  bool success = replay->load();
  if (!success) {
    fprintf(stderr, "Failed to load route: %s\n", route.c_str());
  }
  return success;
}

bool ReplayStream::eventFilter(const Event *event) {
  if (event->which == cereal::Event::Which::CAN) {
    double current_sec = toSeconds(event->mono_time);
    capnp::FlatArrayMessageReader reader(event->data);
    auto e = reader.getRoot<cereal::Event>();
    for (const auto &c : e.getCan()) {
      MessageId id = {.source = c.getSrc(), .address = c.getAddress()};
      const auto dat = c.getDat();
      updateEvent(id, current_sec, (const uint8_t*)dat.begin(), dat.size());
    }
  }

  double ts = millis_since_boot();
  if ((ts - prev_update_ts_) > (1000.0 / settings.fps)) {
    requestUpdateLastMsgs();
    prev_update_ts_ = ts;
  }
  return true;
}

void ReplayStream::pollUpdates() {
  if (merge_pending_.load(std::memory_order_acquire)) {
    merge_pending_.store(false, std::memory_order_relaxed);
    mergeSegments();
    {
      std::lock_guard lk(merge_mutex_);
      merge_done_ = true;
    }
    merge_cv_.notify_one();
  }
  if (seek_pending_.load(std::memory_order_acquire)) {
    seek_pending_.store(false, std::memory_order_relaxed);
    double sec;
    {
      std::lock_guard lk(seek_mutex_);
      sec = seek_target_sec_;
    }
    for (auto &cb : on_seeked_to) cb(sec);
    updateLastMsgsTo(sec);
  }
  AbstractStream::pollUpdates();
}

std::vector<std::shared_ptr<LogReader>> ReplayStream::drainQlogs() {
  std::lock_guard lk(qlog_mutex_);
  std::vector<std::shared_ptr<LogReader>> result;
  result.swap(pending_qlogs_);
  return result;
}

void ReplayStream::pause(bool pause) {
  replay->pause(pause);
  if (pause) {
    for (auto &cb : on_paused) cb();
  } else {
    for (auto &cb : on_resume) cb();
  }
}
