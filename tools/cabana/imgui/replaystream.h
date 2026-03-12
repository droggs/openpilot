#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <set>

#include "common/prefix.h"
#include "tools/cabana/imgui/stream.h"
#include "tools/replay/replay.h"

class ReplayStream : public AbstractStream {
public:
  ReplayStream();
  ~ReplayStream();
  void start() override { replay->start(); }
  bool loadRoute(const std::string &route, const std::string &data_dir, uint32_t replay_flags = REPLAY_FLAG_NONE, bool auto_source = false);
  bool eventFilter(const Event *event);
  void seekTo(double ts) override { replay->seekTo(std::max(double(0), ts), false); }
  bool liveStreaming() const override { return false; }
  inline std::string routeName() const override { return replay->route().name(); }
  inline std::string carFingerprint() const override { return replay->carFingerprint(); }
  double minSeconds() const override { return replay->minSeconds(); }
  double maxSeconds() const override { return replay->maxSeconds(); }
  inline double beginDateTimeSecs() const override { return static_cast<double>(replay->routeDateTime()); }
  inline uint64_t beginMonoTime() const override { return replay->routeStartNanos(); }
  inline void setSpeed(float speed) override { replay->setSpeed(speed); }
  inline double getSpeed() override { return replay->getSpeed(); }
  inline Replay *getReplay() const { return replay.get(); }
  inline bool isPaused() const override { return replay->isPaused(); }
  void pause(bool pause) override;
  void pollUpdates() override;
  void registerCallbacks() override;

private:
  void mergeSegments();
  std::unique_ptr<Replay> replay = nullptr;
  std::set<int> processed_segments;
  std::unique_ptr<OpenpilotPrefix> op_prefix;

  // BlockingQueuedConnection replacement: segment manager thread sets flag and waits,
  // main thread's pollUpdates() calls mergeSegments() and signals completion.
  std::mutex merge_mutex_;
  std::condition_variable merge_cv_;
  std::atomic<bool> merge_pending_ = false;
  bool merge_done_ = false;

  // Deferred seek: replay thread stores target sec (non-blocking),
  // UI thread processes in pollUpdates(). Multiple seeks are coalesced.
  std::mutex seek_mutex_;
  std::atomic<bool> seek_pending_ = false;
  double seek_target_sec_ = 0.0;

  // Qlog forwarding: Timeline loads qlogs separately (unfiltered) and passes
  // them via onQLogLoaded. We queue them here for the UI thread to consume.
  std::mutex qlog_mutex_;
  std::vector<std::shared_ptr<LogReader>> pending_qlogs_;

public:
  // Called by UI thread to drain any newly-loaded qlogs (for thumbnail parsing)
  std::vector<std::shared_ptr<LogReader>> drainQlogs();

private:
  double prev_update_ts_ = 0;
};
