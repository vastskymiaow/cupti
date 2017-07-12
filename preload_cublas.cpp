
#include <cassert>
#include <cstdio>
#include <dlfcn.h>
#include <list>

#include <cublas_v2.h>

#include "allocations.hpp"
#include "callbacks.hpp"
#include "driver_state.hpp"
#include "thread.hpp"
#include "values.hpp"

typedef cublasStatus_t (*cublasDgemvFunc)(cublasHandle_t, cublasOperation_t,
                                          int, int, const double *,
                                          const double *, int, const double *,
                                          int, const double *, double *, int);
extern "C" cublasStatus_t cublasDgemv(cublasHandle_t handle,
                                      cublasOperation_t trans, int m, int n,
                                      const double *alpha, const double *A,
                                      int lda, const double *x, int incx,
                                      const double *beta, double *y, int incy) {
  static cublasDgemvFunc real_cublasDgemv = nullptr;
  printf("prof.so intercepted cublasDgemv call\n");

  if (real_cublasDgemv == nullptr) {
    real_cublasDgemv = (cublasDgemvFunc)dlsym(RTLD_NEXT, "cublasDgemv_v2");
  }
  assert(real_cublasDgemv && "Will the real cublasDgemv please stand up?");

  // record data, we know things about how this API works
  auto &values = Values::instance();

  // Find the argument values
  // http://docs.nvidia.com/cuda/cublas/index.html#cublas-lt-t-gt-gemv
  Values::id_type aKey, xKey, yKey;
  Values::value_type aVal, xVal, yVal;
  std::tie(aKey, aVal) = values.find_live_device((uintptr_t)A, 1);
  std::tie(xKey, xVal) = values.find_live_device((uintptr_t)x, 1);
  std::tie(yKey, yVal) = values.find_live_device((uintptr_t)y, 1);

  assert(aKey && xKey && yKey &&
         "Couldn't find Dgemv argument value on device");

  // FIXME: could use these to do better on dependences
  printf("WARN: not handling some values (A, alpha, beta)\n");

  const auto newValue =
      std::shared_ptr<Value>(new Value(*yVal)); // duplicate the value
  values.insert(newValue);
  newValue->add_depends_on(aKey);
  newValue->add_depends_on(xKey);
  newValue->add_depends_on(yKey);

  DriverState::this_thread().pause_cupti_callbacks();
  printf("WARN: disabling CUPTI callbacks during cublasDgemv "
         "call\n");
  const cublasStatus_t ret = real_cublasDgemv(handle, trans, m, n, alpha, A,
                                              lda, x, incx, beta, y, incy);
  DriverState::this_thread().resume_cupti_callbacks();

  return ret;
}

typedef cublasStatus_t (*cublasSdotFunc)(cublasHandle_t handle, int n,
                                         const float *x, int incx,
                                         const float *y, int incy,
                                         float *result);
extern "C" cublasStatus_t cublasSdot(cublasHandle_t handle, int n,
                                     const float *x, int incx, const float *y,
                                     int incy, float *result) {
  static cublasSdotFunc real_cublasSdot = nullptr;
  printf("prof.so intercepted cublasSdot call\n");

  if (real_cublasSdot == nullptr) {
    real_cublasSdot = (cublasSdotFunc)dlsym(RTLD_NEXT, "cublasSdot_v2");
  }
  assert(real_cublasSdot && "Will the real cublasSdot please stand up?");

  // record data, we know things about how this API works
  auto &values = Values::instance();
  auto &allocations = Allocations::instance();

  // Find the argument values
  // http://docs.nvidia.com/cuda/cublas/index.html#cublas-lt-t-gt-gemv
  Values::id_type xId, yId;
  printf("Looking for x=%lu\n", (uintptr_t)x);
  std::tie(xId, std::ignore) =
      values.find_live((uintptr_t)x, AddressSpace::Cuda());
  assert(xId && "Couldn't find cublasSdot x argument value on device");
  std::tie(yId, std::ignore) =
      values.find_live((uintptr_t)y, AddressSpace::Cuda());
  assert(yId && "Couldn't find cublasSdot y argument value on device");

  // see if we can find an allocation for the result
  printf("Looking for allocation result=%lu\n", (uintptr_t)result);
  Allocations::id_type rAllocId;
  std::tie(rAllocId, std::ignore) = allocations.find_live(
      (uintptr_t)result, sizeof(float), AddressSpace::Cuda());

  if (rAllocId == Allocations::noid) {
    printf("WARN: creating implicit allocation for cublasSdot result\n");
    Memory AM = Memory(Memory::Unknown);
    auto pair = allocations.insert(std::shared_ptr<AllocationRecord>(
        new AllocationRecord((uintptr_t)result, sizeof(float),
                             AddressSpace::Cuda(), AM,
                             AllocationRecord::PageType::Unknown)));
    assert(pair.second);
    rAllocId = pair.first->first;
  }
  printf("result allocId=%lu\n", rAllocId);
  // Make a new value
  Values::id_type rId;
  Values::value_type rVal;
  std::tie(rId, rVal) =
      values.new_value((uintptr_t)result, sizeof(float), rAllocId);
  rVal->add_depends_on(xId);

  DriverState::this_thread().pause_cupti_callbacks();
  printf("WARN: disabling CUPTI callbacks during cublasSdot call\n");
  const cublasStatus_t ret =
      real_cublasSdot(handle, n, x, incx, y, incy, result);
  DriverState::this_thread().resume_cupti_callbacks();
  return ret;
}

typedef cublasStatus_t (*cublasSasumFunc)(cublasHandle_t, int, const float *,
                                          int, float *);
extern "C" cublasStatus_t cublasSasum(cublasHandle_t handle, int n,
                                      const float *x, int incx, float *result) {
  static cublasSasumFunc real_cublasSasum = nullptr;
  printf("prof.so intercepted cublasSasum call\n");

  if (real_cublasSasum == nullptr) {
    real_cublasSasum = (cublasSasumFunc)dlsym(RTLD_NEXT, "cublasSasum_v2");
  }
  assert(real_cublasSasum && "Will the real cublasSasum please stand up?");

  // record data, we know things about how this API works
  auto &values = Values::instance();
  auto &allocations = Allocations::instance();

  // Find the argument values
  // http://docs.nvidia.com/cuda/cublas/index.html#cublas-lt-t-gt-gemv
  Values::id_type xId;
  std::tie(xId, std::ignore) =
      values.find_live((uintptr_t)x, AddressSpace::Cuda());
  assert(xId && "Couldn't find Sasum x argument value on device");

  // see if we can find an allocation for the result
  Allocations::id_type rAllocId;
  std::tie(rAllocId, std::ignore) = allocations.find_live(
      (uintptr_t)result, sizeof(float), AddressSpace::Cuda());

  if (!rAllocId) {
    // FIXME - can we do a better job with some parameters here
    Memory AM(Memory::Unknown);
    std::tie(rAllocId, std::ignore) = allocations.new_allocation(
        (uintptr_t)result, sizeof(float), AddressSpace::Cuda(), AM,
        AllocationRecord::PageType::Unknown);
    printf("WARN: new allocId=%lu for result=%lu\n", rAllocId,
           (uintptr_t)result);
  }
  assert(rAllocId && "If there is no allocation, we need to make one");

  // Make a new value
  Values::id_type rId;
  Values::value_type rVal;
  std::tie(rId, rVal) =
      values.new_value((uintptr_t)result, sizeof(float), rAllocId);
  rVal->add_depends_on(xId);

  DriverState::this_thread().pause_cupti_callbacks();
  printf("WARN: tid=%d disabling CUPTI callbacks during cublasSasum call\n",
         get_thread_id());
  const cublasStatus_t ret = real_cublasSasum(handle, n, x, incx, result);
  DriverState::this_thread().resume_cupti_callbacks();
  return ret;
}

typedef cublasStatus_t (*cublasDestroyFunc)(cublasHandle_t handle);
extern "C" cublasStatus_t cublasDestroy(cublasHandle_t handle) {
  static cublasDestroyFunc real_cublasDestroy = nullptr;
  printf("prof.so intercepted cublasDestroy call\n");

  if (real_cublasDestroy == nullptr) {
    real_cublasDestroy =
        (cublasDestroyFunc)dlsym(RTLD_NEXT, "cublasDestroy_v2");
  }
  assert(real_cublasDestroy && "Will the real cublasDestroy please stand up?");

  DriverState::this_thread().pause_cupti_callbacks();
  printf("WARN: tid=%d disabling CUPTI callbacks during cublasDestroy call\n",
         get_thread_id());
  const cublasStatus_t ret = real_cublasDestroy(handle);
  DriverState::this_thread().resume_cupti_callbacks();
  return ret;
}

typedef cublasStatus_t (*cublasCreateFunc)(cublasHandle_t *handle);
extern "C" cublasStatus_t cublasCreate(cublasHandle_t *handle) {
  static cublasCreateFunc real_cublasCreate = nullptr;
  onceActivateCallbacks();
  printf("prof.so intercepted cublasCreate call\n");

  if (real_cublasCreate == nullptr) {
    real_cublasCreate = (cublasCreateFunc)dlsym(RTLD_NEXT, "cublasCreate_v2");
  }
  assert(real_cublasCreate && "Will the real cublasCreate please stand up?");

  printf("WARN: disabling CUPTI callbacks during cublasCreate call\n");
  DriverState::this_thread().pause_cupti_callbacks();
  const cublasStatus_t ret = real_cublasCreate(handle);
  DriverState::this_thread().resume_cupti_callbacks();
  return ret;
}