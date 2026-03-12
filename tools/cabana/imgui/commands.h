#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "tools/cabana/imgui/dbcmanager.h"
#include "tools/cabana/imgui/stream.h"

class CabanaUndoCommand {
public:
  virtual ~CabanaUndoCommand() = default;
  virtual void undo() = 0;
  virtual void redo() = 0;
  std::string text() const { return text_; }
  void setText(const std::string &t) { text_ = t; }
protected:
  std::string text_;
};

class CabanaUndoStack {
public:
  void push(CabanaUndoCommand *cmd);
  void undo();
  void redo();
  bool canUndo() const { return index_ > 0; }
  bool canRedo() const { return index_ < (int)commands_.size(); }
  std::string undoText() const { return canUndo() ? commands_[index_ - 1]->text() : ""; }
  std::string redoText() const { return canRedo() ? commands_[index_]->text() : ""; }
  bool isClean() const { return index_ == clean_index_; }
  int index() const { return index_; }
  void setClean() { clean_index_ = index_; }
  void clear();
  int count() const { return static_cast<int>(commands_.size()); }
  std::string text(int i) const { return (i >= 0 && i < (int)commands_.size()) ? commands_[i]->text() : ""; }

private:
  std::vector<std::unique_ptr<CabanaUndoCommand>> commands_;
  int index_ = 0;
  int clean_index_ = 0;
};

class EditMsgCommand : public CabanaUndoCommand {
public:
  EditMsgCommand(const MessageId &id, const std::string &name, int size,
                 const std::string &node, const std::string &comment);
  void undo() override;
  void redo() override;
private:
  const MessageId id;
  std::string old_name, new_name, old_comment, new_comment, old_node, new_node;
  int old_size = 0, new_size = 0;
};

class RemoveMsgCommand : public CabanaUndoCommand {
public:
  RemoveMsgCommand(const MessageId &id);
  void undo() override;
  void redo() override;
private:
  const MessageId id;
  cabana::Msg message;
};

class AddSigCommand : public CabanaUndoCommand {
public:
  AddSigCommand(const MessageId &id, const cabana::Signal &sig);
  void undo() override;
  void redo() override;
private:
  const MessageId id;
  bool msg_created = false;
  cabana::Signal signal = {};
};

class RemoveSigCommand : public CabanaUndoCommand {
public:
  RemoveSigCommand(const MessageId &id, const cabana::Signal *sig);
  void undo() override;
  void redo() override;
private:
  const MessageId id;
  std::vector<cabana::Signal> sigs;
};

class EditSignalCommand : public CabanaUndoCommand {
public:
  EditSignalCommand(const MessageId &id, const cabana::Signal *sig, const cabana::Signal &new_sig);
  void undo() override;
  void redo() override;
private:
  const MessageId id;
  std::vector<std::pair<cabana::Signal, cabana::Signal>> sigs;
};

namespace UndoStack {
  CabanaUndoStack *instance();
  inline void push(CabanaUndoCommand *cmd) { instance()->push(cmd); }
};
