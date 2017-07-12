#ifndef DRIVER_STATE_HPP
#define DRIVER_STATE_HPP

#include <cuda.h>
#include <cuda_runtime.h>
#include <cupti.h>

#include <map>
#include <vector>

#include "thread.hpp"

class APICallRecord {
private:
  CUpti_CallbackDomain domain_;
  CUpti_CallbackId cbid_;
  const CUpti_CallbackData *cbInfo_;

public:
  bool is_runtime() const { return domain_ == CUPTI_CB_DOMAIN_RUNTIME_API; }
  CUpti_CallbackDomain domain() const { return domain_; }
  CUpti_CallbackId cbid() const { return cbid_; }
  const CUpti_CallbackData *cb_info() const { return cbInfo_; }
  APICallRecord()
      : domain_(CUPTI_CB_DOMAIN_INVALID), cbid_(-1), cbInfo_(nullptr) {}
  APICallRecord(const CUpti_CallbackDomain domain, const CUpti_CallbackId cbid,
                const CUpti_CallbackData *cbInfo)
      : domain_(domain), cbid_(cbid), cbInfo_(cbInfo) {}
};

class ThreadState {
private:
  int currentDevice_;
  bool cuptiCallbacksEnabled_;

  std::vector<APICallRecord> apiStack_;

public:
  ThreadState() : cuptiCallbacksEnabled_(true) {}

  int current_device() const { return currentDevice_; }
  void set_device(const int device) { currentDevice_ = device; }

  void api_enter(const CUpti_CallbackDomain domain, const CUpti_CallbackId cbid,
                 const CUpti_CallbackData *cbInfo);
  void api_exit(const CUpti_CallbackDomain domain, const CUpti_CallbackId cbid,
                const CUpti_CallbackData *cbInfo);

  bool in_child_api() const { return apiStack_.size() >= 2; }
  const APICallRecord &parent_api() const;

  void pause_cupti_callbacks();
  void resume_cupti_callbacks();

  bool is_cupti_callbacks_enabled() const { return cuptiCallbacksEnabled_; }
};

// FIXME: not thread-safe
class DriverState {
public:
  typedef ThreadState mapped_type;
  typedef tid_t key_type;
  typedef std::pair<key_type, mapped_type> value_type;

private:
  typedef std::map<key_type, mapped_type> ThreadMap;
  ThreadMap threadStates_;

  static DriverState &instance();

public:
  static mapped_type &this_thread() {
    return instance().threadStates_[get_thread_id()];
  }
};

#endif
