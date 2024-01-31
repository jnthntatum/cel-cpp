// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_BASE_MEMORY_H_
#define THIRD_PARTY_CEL_CPP_BASE_MEMORY_H_

#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "common/memory.h"  // IWYU pragma: export

namespace cel {

template <typename T>
class Allocator;

// STL allocator implementation which is backed by MemoryManager.
template <typename T>
class Allocator {
 public:
  using value_type = T;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using propagate_on_container_move_assignment = std::true_type;
  using is_always_equal = std::false_type;

  template <typename U>
  struct rebind final {
    using other = Allocator<U>;
  };

  explicit Allocator(
      ABSL_ATTRIBUTE_LIFETIME_BOUND MemoryManagerRef memory_manager)
      : memory_manager_(memory_manager),
        allocation_only_(memory_manager.memory_management() ==
                         MemoryManagement::kPooling) {}

  Allocator(const Allocator&) = default;

  template <typename U>
  Allocator(const Allocator<U>& other)  // NOLINT(google-explicit-constructor)
      : memory_manager_(other.memory_manager_),
        allocation_only_(other.allocation_only_) {}

  pointer allocate(size_type n) {
    if (!allocation_only_) {
      return static_cast<pointer>(::operator new(
          n * sizeof(T), static_cast<std::align_val_t>(alignof(T))));
    }
    if (memory_manager_.pointer_ == nullptr) {
      return static_cast<pointer>(
          static_cast<PoolingMemoryManager*>(memory_manager_.vpointer_)
              ->Allocate(n * sizeof(T), alignof(T)));
    } else {
      return static_cast<pointer>(
          static_cast<const PoolingMemoryManagerVirtualTable*>(
              memory_manager_.vpointer_)
              ->Allocate(memory_manager_.pointer_, n * sizeof(T), alignof(T)));
    }
  }

  pointer allocate(size_type n, const void* hint) {
    static_cast<void>(hint);
    return allocate(n);
  }

  void deallocate(pointer p, size_type n) {
    if (!allocation_only_) {
      ::operator delete(static_cast<void*>(p), n * sizeof(T),
                        static_cast<std::align_val_t>(alignof(T)));
    }
  }

  constexpr size_type max_size() const noexcept {
    return std::numeric_limits<size_type>::max() / sizeof(value_type);
  }

  pointer address(reference x) const noexcept { return std::addressof(x); }

  const_pointer address(const_reference x) const noexcept {
    return std::addressof(x);
  }

  void construct(pointer p, const_reference val) {
    ::new (static_cast<void*>(p)) T(val);
  }

  template <typename U, typename... Args>
  void construct(U* p, Args&&... args) {
    ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
  }

  void destroy(pointer p) { p->~T(); }

  template <typename U>
  void destroy(U* p) {
    p->~U();
  }

  template <typename U>
  bool operator==(const Allocator<U>& rhs) const {
    return &memory_manager_ == &rhs.memory_manager_;
  }

  template <typename U>
  bool operator!=(const Allocator<U>& rhs) const {
    return &memory_manager_ != &rhs.memory_manager_;
  }

 private:
  template <typename U>
  friend class Allocator;

  MemoryManagerRef memory_manager_;
  // Ugh. This is here because of legacy behavior. MemoryManagerRef is
  // guaranteed to exist during allocation, but not necessarily during
  // deallocation. So we store the member variable from MemoryManager. This can
  // go away once CelValue and friends are entirely gone and everybody is
  // instantiating their own MemoryManager.
  bool allocation_only_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_MEMORY_H_
