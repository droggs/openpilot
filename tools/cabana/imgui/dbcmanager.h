#pragma once

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "tools/cabana/imgui/dbcfile.h"

typedef std::set<int> SourceSet;
const SourceSet SOURCE_ALL = {-1};
const int INVALID_SOURCE = 0xff;
inline bool operator<(const std::shared_ptr<DBCFile> &l, const std::shared_ptr<DBCFile> &r) { return l.get() < r.get(); }

class DBCManager {
public:
  DBCManager() {}
  ~DBCManager() {}
  bool open(const SourceSet &sources, const std::string &dbc_file_name, std::string *error = nullptr);
  bool open(const SourceSet &sources, const std::string &name, const std::string &content, std::string *error = nullptr);
  void close(const SourceSet &sources);
  void close(DBCFile *dbc_file);
  void closeAll();

  void addSignal(const MessageId &id, const cabana::Signal &sig);
  void updateSignal(const MessageId &id, const std::string &sig_name, const cabana::Signal &sig);
  void removeSignal(const MessageId &id, const std::string &sig_name);

  void updateMsg(const MessageId &id, const std::string &name, uint32_t size, const std::string &node, const std::string &comment);
  void removeMsg(const MessageId &id);

  std::string newMsgName(const MessageId &id);
  std::string newSignalName(const MessageId &id);

  const std::map<uint32_t, cabana::Msg> &getMessages(uint8_t source);
  cabana::Msg *msg(const MessageId &id);
  cabana::Msg *msg(uint8_t source, const std::string &name);

  std::vector<std::string> signalNames();
  inline int dbcCount() { return allDBCFiles().size(); }
  int nonEmptyDBCCount();

  const SourceSet sources(const DBCFile *dbc_file) const;
  DBCFile *findDBCFile(const uint8_t source);
  inline DBCFile *findDBCFile(const MessageId &id) { return findDBCFile(id.source); }
  std::set<DBCFile *> allDBCFiles();

  // Callback registration (replaces Qt signals)
  using VoidCb = std::function<void()>;
  using SignalAddedCb = std::function<void(MessageId, const cabana::Signal*)>;
  using SignalRemovedCb = std::function<void(MessageId, const cabana::Signal*)>;
  using SignalUpdatedCb = std::function<void(MessageId, const std::string &old_name, const cabana::Signal*)>;
  using MsgUpdatedCb = std::function<void(MessageId)>;
  using MsgRemovedCb = std::function<void(MessageId)>;

  void onSignalAdded(SignalAddedCb cb) { on_signal_added_.push_back(std::move(cb)); }
  void onSignalRemoved(SignalRemovedCb cb) { on_signal_removed_.push_back(std::move(cb)); }
  void onSignalUpdated(SignalUpdatedCb cb) { on_signal_updated_.push_back(std::move(cb)); }
  void onMsgUpdated(MsgUpdatedCb cb) { on_msg_updated_.push_back(std::move(cb)); }
  void onMsgRemoved(MsgRemovedCb cb) { on_msg_removed_.push_back(std::move(cb)); }
  void onDBCFileChanged(VoidCb cb) { on_dbc_file_changed_.push_back(std::move(cb)); }
  void onMaskUpdated(VoidCb cb) { on_mask_updated_.push_back(std::move(cb)); }
  void clearCallbacks() {
    on_signal_added_.clear();
    on_signal_removed_.clear();
    on_signal_updated_.clear();
    on_msg_updated_.clear();
    on_msg_removed_.clear();
    on_dbc_file_changed_.clear();
    on_mask_updated_.clear();
  }

private:
  std::map<int, std::shared_ptr<DBCFile>> dbc_files;

  std::vector<SignalAddedCb> on_signal_added_;
  std::vector<SignalRemovedCb> on_signal_removed_;
  std::vector<SignalUpdatedCb> on_signal_updated_;
  std::vector<MsgUpdatedCb> on_msg_updated_;
  std::vector<MsgRemovedCb> on_msg_removed_;
  std::vector<VoidCb> on_dbc_file_changed_;
  std::vector<VoidCb> on_mask_updated_;
};

DBCManager *dbc();

std::string toString(const SourceSet &ss);
inline std::string msgName(const MessageId &id) {
  auto msg = dbc()->msg(id);
  return msg ? msg->name : UNTITLED;
}
