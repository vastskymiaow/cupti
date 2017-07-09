#ifndef ALLOCATIONRECORD_HPP
#define ALLOCATIONRECORD_HPP

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>

#include "address_space.hpp"
#include "extent.hpp"

class AllocationRecord : public Extent {
public:
  enum class PageType { Pinned, Pageable, Unknown };
  typedef uintptr_t id_type;

private:
  AddressSpace address_space_;
  PageType type_;
  tid_t thread_id_;

public:
  friend std::ostream &operator<<(std::ostream &os, const AllocationRecord &v);
  AllocationRecord(uintptr_t pos, size_t size, AddressSpace as, PageType pt)
      : Extent(pos, size), address_space_(as), type_(pt) {}

  std::string json() const;

  bool overlaps(const AllocationRecord &other) {
    return address_space_.overlaps(other.address_space_) &&
           Extent::overlaps(other);
  }

  bool contains(const AllocationRecord &other) {
    return address_space_.overlaps(other.address_space_) &&
           Extent::contains(other);
  }

  id_type Id() const { return reinterpret_cast<id_type>(this); }
  AddressSpace address_space() const { return address_space_; }
};

#endif
