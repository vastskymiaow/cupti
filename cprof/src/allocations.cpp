#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <cassert>

#include "cprof/allocations.hpp"

using boost::property_tree::ptree;
using boost::property_tree::read_json;
using boost::property_tree::write_json;
using namespace boost::icl;
using cprof::model::Location;
using cprof::model::Memory;

namespace cprof {

Allocation Allocations::unsafe_find(uintptr_t pos, size_t size,
                                    const AddressSpace &as) {
  assert(pos && "No allocations at null pointer");
  std::lock_guard<std::mutex> guard(access_mutex_);
  auto allocationsIter = addrSpaceAllocs_.find(as);
  if (allocationsIter != addrSpaceAllocs_.end()) {
    auto &allocations = allocationsIter->second;

    // std::cerr << "looking for " <<q pos << " +" << size << "\n";
    auto si = interval<uintptr_t>::right_open(pos, pos + size);
    auto ai = allocations.find(si);
    if (ai != allocations.end()) {
      // std::cerr << "matching allocation at " << ai->second.pos() << "\n";
      return ai->second;
    } else {
      // std::cerr << "no matching alloc\n";
      return nullptr;
    }
  } else {
    // std::cerr << "no matching AS\n";
    return nullptr;
  }
}

Allocation Allocations::free(uintptr_t pos, const AddressSpace &as) {
  assert(pos && "No allocations at null pointer");
  std::lock_guard<std::mutex> guard(access_mutex_);
  auto allocationsIter = addrSpaceAllocs_.find(as);
  if (allocationsIter != addrSpaceAllocs_.end()) {
    auto &allocations = allocationsIter->second;
    auto ai = allocations.find(pos);
    if (ai != allocations.end()) {
      if (ai->second.pos() == pos) {
        auto alloc = ai->second;
        alloc.free();
        return alloc;
      }
    }
  }
  return nullptr;
}

Allocation Allocations::insert(const Allocation &a) {
  logging::atomic_out(a.json());
  auto &allocs = addrSpaceAllocs_[a.address_space()];
  allocs += std::make_pair(a.interval(), a);
  return allocs.find(a.interval())->second;
}

Allocation Allocations::new_allocation(uintptr_t pos, size_t size,
                                       const AddressSpace &as, const Memory &am,
                                       const Location &al) {
  if (size == 0) {
    logging::err() << "WARN: creating size 0 allocation" << std::endl;
  }
  std::lock_guard<std::mutex> guard(access_mutex_);
  auto ar = new AllocationRecord(pos, size, as, am, al);
  return insert(Allocation(ar));
}

Value Allocations::find_value(const uintptr_t pos, const size_t size,
                              const AddressSpace &as) {
  std::lock_guard<std::mutex> guard(access_mutex_);
  logging::err() << "INFO: Looking for value @ [" << pos << ", +" << size << ")"
                 << std::endl;

  const auto &alloc = unsafe_find(pos, size, as);
  const auto val = alloc.value(pos, size);
  return val;
}

Value Allocations::new_value(const uintptr_t pos, const size_t size,
                             const AddressSpace &as, const bool initialized) {
  std::lock_guard<std::mutex> guard(access_mutex_);

  // Find the allocation
  auto alloc = unsafe_find(pos, size, as);
  assert(alloc && "Allocation should be valid");

  return alloc.new_value(pos, size, initialized);
}

} // namespace cprof
