#include <boost/any.hpp>
#include <iostream>
#include <thread>
#include <vector>

#include "cprof/chrome_tracing/complete_event.hpp"
#include "cprof/model/thread.hpp"
#include "cprof/util_cupti.hpp"

#include "cupti_activity.hpp"
#include "profiler.hpp"
#include "timer.hpp"

namespace cupti_activity_config {
size_t localBufferSize = 1 * 1024 * 1024;
size_t attrDeviceBufferSize = 8 * 1024 * 1024;
size_t attrDeviceBufferPoolLimit = 2;
size_t attrValueSize_size_t = sizeof(size_t);

std::vector<ActivityHandler> activityHandlers;

size_t *attr_value_size(const CUpti_ActivityAttribute &attr) {
  return &attrValueSize_size_t; // all attributes are size_t as of CUDA 9.1
}

void set_local_buffer_size(const size_t bytes) { localBufferSize = bytes; }
void set_device_buffer_size(const size_t bytes) {
  attrDeviceBufferSize = bytes;
}
void set_device_buffer_pool_limit(const size_t npools) {
  attrDeviceBufferPoolLimit = npools;
}
size_t local_buffer_size() { return localBufferSize; }
size_t align_size() { return 8; }
size_t *attr_device_buffer_size() { return &attrDeviceBufferSize; }
size_t *attr_device_buffer_pool_limit() { return &attrDeviceBufferPoolLimit; }

void set_activity_handler(ActivityHandler fn) {
  activityHandlers.clear();
  activityHandlers.push_back(fn);
}

void add_activity_handler(ActivityHandler fn) {
  activityHandlers.push_back(fn);
}

void call_activity_handlers(const CUpti_Activity *record) {
  assert(record);
  for (const auto &f : activityHandlers) {
    assert(f);
    f(record);
  }
}

} // namespace cupti_activity_config

#define ALIGN_BUFFER(buffer, align)                                            \
  (((uintptr_t)(buffer) & ((align)-1))                                         \
       ? ((buffer) + (align) - ((uintptr_t)(buffer) & ((align)-1)))            \
       : (buffer))

void CUPTIAPI cuptiActivityBufferRequested(uint8_t **buffer, size_t *size,
                                           size_t *maxNumRecords) {

  using cupti_activity_config::align_size;
  using cupti_activity_config::local_buffer_size;

  uint8_t *rawBuffer;

  *size = local_buffer_size();
  rawBuffer = (uint8_t *)malloc(*size + align_size());

  *buffer = ALIGN_BUFFER(rawBuffer, align_size());
  *maxNumRecords = 0; // as many records as possible

  if (*buffer == NULL) {
    profiler::err() << "ERROR: out of memory" << std::endl;
    exit(-1);
  }
}

void threadFunc(uint8_t *localBuffer, size_t validSize) {

  using cupti_activity_config::call_activity_handlers;

  auto start = cprof::now();

  CUpti_Activity *record = NULL;
  if (validSize > 0) {
    do {
      auto err = cuptiActivityGetNextRecord(localBuffer, validSize, &record);
      if (err == CUPTI_ERROR_MAX_LIMIT_REACHED) {
        break;
      }

      CUPTI_CHECK(err, profiler::err());
      profiler::timer().activity_add_annotations(record);
      call_activity_handlers(record);
    } while (true);
  }

  auto end = cprof::now();

  auto event = cprof::chrome_tracing::CompleteEventNs(
      "", {}, cprof::nanos(start), (cprof::nanos(end) - cprof::nanos(start)),
      "profiler", "cupti record handler");

  Profiler::instance().chrome_tracer().write_event(event);
}

void CUPTIAPI cuptiActivityBufferCompleted(CUcontext ctx, uint32_t streamId,
                                           uint8_t *buffer, size_t size,
                                           size_t validSize) {

  span_t activity_span;
  if (Profiler::instance().is_zipkin_enabled()) {
    activity_span = Profiler::instance().overheadTracer_->StartSpan(
        "Activity API",
        {FollowsFrom(&Profiler::instance().rootSpan_->context())});
  }
  // uint8_t *localBuffer;
  // localBuffer = (uint8_t *)malloc(BUFFER_SIZE * 1024 + ALIGN_SIZE);
  // ALIGN_BUFFER(localBuffer, ALIGN_SIZE);
  // memcpy(localBuffer, buffer, validSize);
  // threadFunc()
  threadFunc(buffer, validSize);
  if (Profiler::instance().is_zipkin_enabled()) {
    activity_span->Finish();
  }
}