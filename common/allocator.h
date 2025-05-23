// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_ALLOCATOR_H_
#define THIRD_PARTY_CEL_CPP_COMMON_ALLOCATOR_H_

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/log/die_if_null.h"
#include "absl/numeric/bits.h"
#include "common/arena.h"
#include "common/data.h"
#include "internal/new.h"
#include "google/protobuf/arena.h"

namespace cel {

enum class AllocatorKind {
  kArena = 1,
  kNewDelete = 2,
};

template <typename S>
void AbslStringify(S& sink, AllocatorKind kind) {
  switch (kind) {
    case AllocatorKind::kArena:
      sink.Append("ARENA");
      return;
    case AllocatorKind::kNewDelete:
      sink.Append("NEW_DELETE");
      return;
    default:
      sink.Append("ERROR");
      return;
  }
}

template <typename T = void>
class NewDeleteAllocator;
template <typename T = void>
class ArenaAllocator;
template <typename T = void>
class Allocator;

// `NewDeleteAllocator<>` is a type-erased vocabulary type capable of performing
// allocation/deallocation and construction/destruction using memory owned by
// `operator new`.
template <>
class NewDeleteAllocator<void> {
 public:
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using propagate_on_container_copy_assignment = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;
  using is_always_equal = std::true_type;

  NewDeleteAllocator() = default;
  NewDeleteAllocator(const NewDeleteAllocator&) = default;
  NewDeleteAllocator& operator=(const NewDeleteAllocator&) = default;

  template <typename U, typename = std::enable_if_t<!std::is_void_v<U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr NewDeleteAllocator(
      [[maybe_unused]] const NewDeleteAllocator<U>& other) noexcept {}

  // Allocates at least `nbytes` bytes with a minimum alignment of `alignment`
  // from the underlying memory resource. When the underlying memory resource is
  // `operator new`, `deallocate_bytes` must be called at some point, otherwise
  // calling `deallocate_bytes` is optional. The caller must not pass an object
  // constructed in the return memory to `delete_object`, doing so is undefined
  // behavior.
  ABSL_MUST_USE_RESULT void* allocate_bytes(
      size_type nbytes, size_type alignment = alignof(std::max_align_t)) {
    ABSL_DCHECK(absl::has_single_bit(alignment));
    if (nbytes == 0) {
      return nullptr;
    }
    return internal::AlignedNew(nbytes,
                                static_cast<std::align_val_t>(alignment));
  }

  // Deallocates memory previously returned by `allocate_bytes`.
  void deallocate_bytes(
      void* p, size_type nbytes,
      size_type alignment = alignof(std::max_align_t)) noexcept {
    ABSL_DCHECK((p == nullptr && nbytes == 0) || (p != nullptr && nbytes != 0));
    ABSL_DCHECK(absl::has_single_bit(alignment));
    internal::SizedAlignedDelete(p, nbytes,
                                 static_cast<std::align_val_t>(alignment));
  }

  template <typename T>
  ABSL_MUST_USE_RESULT T* allocate_object(size_type n = 1) {
    return static_cast<T*>(allocate_bytes(sizeof(T) * n, alignof(T)));
  }

  template <typename T>
  void deallocate_object(T* p, size_type n = 1) {
    deallocate_bytes(p, sizeof(T) * n, alignof(T));
  }

  // Allocates memory suitable for an object of type `T` and constructs the
  // object by forwarding the provided arguments. If the underlying memory
  // resource is `operator new` is false, `delete_object` must eventually be
  // called.
  template <typename T, typename... Args>
  ABSL_MUST_USE_RESULT T* new_object(Args&&... args) {
    return new T(std::forward<Args>(args)...);
  }

  // Destructs the object of type `T` located at address `p` and deallocates the
  // memory, `p` must have been previously returned by `new_object`.
  template <typename T>
  void delete_object(T* p) noexcept {
    ABSL_DCHECK(p != nullptr);
    delete p;
  }

  void delete_object(std::nullptr_t) = delete;

 private:
  template <typename U>
  friend class NewDeleteAllocator;
};

// `NewDeleteAllocator<T>` is an extension of `NewDeleteAllocator<>` which
// adheres to the named C++ requirements for `Allocator`, allowing it to be used
// in places which accept custom STL allocators.
template <typename T>
class NewDeleteAllocator : public NewDeleteAllocator<void> {
 public:
  static_assert(!std::is_const_v<T>, "T must not be const qualified");
  static_assert(!std::is_volatile_v<T>, "T must not be volatile qualified");
  static_assert(std::is_object_v<T>, "T must be an object type");

  using value_type = T;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using reference = value_type&;
  using const_reference = const value_type&;

  using NewDeleteAllocator<void>::NewDeleteAllocator;

  template <typename U, typename = std::enable_if_t<!std::is_same_v<U, T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr NewDeleteAllocator(
      [[maybe_unused]] const NewDeleteAllocator<U>& other) noexcept {}

  pointer allocate(size_type n, const void* /*hint*/ = nullptr) {
    return reinterpret_cast<pointer>(internal::AlignedNew(
        n * sizeof(T), static_cast<std::align_val_t>(alignof(T))));
  }

#if defined(__cpp_lib_allocate_at_least) && \
    __cpp_lib_allocate_at_least >= 202302L
  std::allocation_result<pointer, size_type> allocate_at_least(size_type n) {
    void* addr;
    size_type size;
    std::tie(addr, size) = internal::SizeReturningAlignedNew(
        n * sizeof(T), static_cast<std::align_val_t>(alignof(T)));
    std::allocation_result<pointer, size_type> result;
    result.ptr = reinterpret_cast<pointer>(addr);
    result.count = size / sizeof(T);
    return result;
  }
#endif

  void deallocate(pointer p, size_type n) noexcept {
    internal::SizedAlignedDelete(p, n * sizeof(T),
                                 static_cast<std::align_val_t>(alignof(T)));
  }

  template <typename U, typename... Args>
  void construct(U* p, Args&&... args) {
    ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
  }

  template <typename U>
  void destroy(U* p) noexcept {
    std::destroy_at(p);
  }
};

template <typename T, typename U>
inline bool operator==(NewDeleteAllocator<T>, NewDeleteAllocator<U>) noexcept {
  return true;
}

template <typename T, typename U>
inline bool operator!=(NewDeleteAllocator<T> lhs,
                       NewDeleteAllocator<U> rhs) noexcept {
  return !operator==(lhs, rhs);
}

NewDeleteAllocator() -> NewDeleteAllocator<void>;
template <typename T>
NewDeleteAllocator(const NewDeleteAllocator<T>&) -> NewDeleteAllocator<T>;

// `ArenaAllocator<>` is a type-erased vocabulary type capable of performing
// allocation/deallocation and construction/destruction using memory owned by
// `google::protobuf::Arena`.
template <>
class ArenaAllocator<void> {
 public:
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using propagate_on_container_copy_assignment = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;

  ArenaAllocator() = delete;

  ArenaAllocator(const ArenaAllocator&) = default;
  ArenaAllocator& operator=(const ArenaAllocator&) = delete;

  ArenaAllocator(std::nullptr_t) = delete;

  template <typename U, typename = std::enable_if_t<!std::is_void_v<U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr ArenaAllocator(const ArenaAllocator<U>& other) noexcept
      : arena_(other.arena()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  ArenaAllocator(google::protobuf::Arena* ABSL_NONNULL arena) noexcept
      : arena_(ABSL_DIE_IF_NULL(arena))  // Crash OK
  {}

  constexpr google::protobuf::Arena* ABSL_NONNULL arena() const noexcept {
    ABSL_ASSUME(arena_ != nullptr);
    return arena_;
  }

  // Allocates at least `nbytes` bytes with a minimum alignment of `alignment`
  // from the underlying memory resource. When the underlying memory resource is
  // `operator new`, `deallocate_bytes` must be called at some point, otherwise
  // calling `deallocate_bytes` is optional. The caller must not pass an object
  // constructed in the return memory to `delete_object`, doing so is undefined
  // behavior.
  ABSL_MUST_USE_RESULT void* allocate_bytes(
      size_type nbytes, size_type alignment = alignof(std::max_align_t)) {
    ABSL_DCHECK(absl::has_single_bit(alignment));
    if (nbytes == 0) {
      return nullptr;
    }
    return arena()->AllocateAligned(nbytes, alignment);
  }

  // Deallocates memory previously returned by `allocate_bytes`.
  void deallocate_bytes(
      void* p, size_type nbytes,
      size_type alignment = alignof(std::max_align_t)) noexcept {
    ABSL_DCHECK((p == nullptr && nbytes == 0) || (p != nullptr && nbytes != 0));
    ABSL_DCHECK(absl::has_single_bit(alignment));
  }

  template <typename T>
  ABSL_MUST_USE_RESULT T* allocate_object(size_type n = 1) {
    return static_cast<T*>(allocate_bytes(sizeof(T) * n, alignof(T)));
  }

  template <typename T>
  void deallocate_object(T* p, size_type n = 1) {
    deallocate_bytes(p, sizeof(T) * n, alignof(T));
  }

  // Allocates memory suitable for an object of type `T` and constructs the
  // object by forwarding the provided arguments. If the underlying memory
  // resource is `operator new` is false, `delete_object` must eventually be
  // called.
  template <typename T, typename... Args>
  ABSL_MUST_USE_RESULT T* new_object(Args&&... args) {
    using U = std::remove_const_t<T>;
    U* object;
    if constexpr (google::protobuf::Arena::is_arena_constructable<U>::value) {
      // Classes derived from `cel::Data` are manually allocated and constructed
      // as those class support determining whether the destructor is skippable
      // at runtime.
      object = google::protobuf::Arena::Create<U>(arena(), std::forward<Args>(args)...);
    } else {
      if constexpr (ArenaTraits<>::constructible<U>()) {
        object = ::new (static_cast<void*>(arena()->AllocateAligned(
            sizeof(U), alignof(U)))) U(arena(), std::forward<Args>(args)...);
      } else {
        object = ::new (static_cast<void*>(arena()->AllocateAligned(
            sizeof(U), alignof(U)))) U(std::forward<Args>(args)...);
      }
      if constexpr (!ArenaTraits<>::always_trivially_destructible<U>()) {
        if (!ArenaTraits<>::trivially_destructible(*object)) {
          arena()->OwnDestructor(object);
        }
      }
    }
    if constexpr (google::protobuf::Arena::is_arena_constructable<U>::value ||
                  std::is_base_of_v<Data, U>) {
      ABSL_DCHECK_EQ(object->GetArena(), arena());
    }
    return object;
  }

  // Destructs the object of type `T` located at address `p` and deallocates the
  // memory, `p` must have been previously returned by `new_object`.
  template <typename T>
  void delete_object(T* p) noexcept {
    using U = std::remove_const_t<T>;
    ABSL_DCHECK(p != nullptr);
    if constexpr (google::protobuf::Arena::is_arena_constructable<U>::value ||
                  std::is_base_of_v<Data, U>) {
      ABSL_DCHECK_EQ(p->GetArena(), arena());
    }
  }

  void delete_object(std::nullptr_t) = delete;

 private:
  template <typename U>
  friend class ArenaAllocator;

  google::protobuf::Arena* ABSL_NONNULL arena_;
};

// `ArenaAllocator<T>` is an extension of `ArenaAllocator<>` which adheres to
// the named C++ requirements for `Allocator`, allowing it to be used in places
// which accept custom STL allocators.
template <typename T>
class ArenaAllocator : public ArenaAllocator<void> {
 private:
  using Base = ArenaAllocator<void>;

 public:
  static_assert(!std::is_const_v<T>, "T must not be const qualified");
  static_assert(!std::is_volatile_v<T>, "T must not be volatile qualified");
  static_assert(std::is_object_v<T>, "T must be an object type");

  using value_type = T;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using reference = value_type&;
  using const_reference = const value_type&;

  using ArenaAllocator<void>::ArenaAllocator;

  template <typename U, typename = std::enable_if_t<!std::is_same_v<U, T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr ArenaAllocator(const ArenaAllocator<U>& other) noexcept
      : Base(other) {}

  pointer allocate(size_type n, const void* /*hint*/ = nullptr) {
    return static_cast<pointer>(
        arena()->AllocateAligned(n * sizeof(T), alignof(T)));
  }

#if defined(__cpp_lib_allocate_at_least) && \
    __cpp_lib_allocate_at_least >= 202302L
  std::allocation_result<pointer, size_type> allocate_at_least(size_type n) {
    std::allocation_result<pointer, size_type> result;
    result.ptr = allocate(n);
    result.count = n;
    return result;
  }
#endif

  void deallocate(pointer, size_type) noexcept {}

  template <typename U, typename... Args>
  void construct(U* p, Args&&... args) {
    static_assert(!google::protobuf::Arena::is_arena_constructable<U>::value);
    ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
  }

  template <typename U>
  void destroy(U* p) noexcept {
    static_assert(!google::protobuf::Arena::is_arena_constructable<U>::value);
    std::destroy_at(p);
  }
};

template <typename T, typename U>
inline bool operator==(ArenaAllocator<T> lhs, ArenaAllocator<U> rhs) noexcept {
  return lhs.arena() == rhs.arena();
}

template <typename T, typename U>
inline bool operator!=(ArenaAllocator<T> lhs, ArenaAllocator<U> rhs) noexcept {
  return !operator==(lhs, rhs);
}

ArenaAllocator(google::protobuf::Arena* ABSL_NONNULL) -> ArenaAllocator<void>;
template <typename T>
ArenaAllocator(const ArenaAllocator<T>&) -> ArenaAllocator<T>;

// `Allocator<>` is a type-erased vocabulary type capable of performing
// allocation/deallocation and construction/destruction using memory owned by
// `google::protobuf::Arena` or `operator new`.
template <>
class Allocator<void> {
 public:
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using propagate_on_container_copy_assignment = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;

  Allocator() = delete;

  Allocator(const Allocator&) = default;
  Allocator& operator=(const Allocator&) = delete;

  Allocator(std::nullptr_t) = delete;

  template <typename U, typename = std::enable_if_t<!std::is_void_v<U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr Allocator(const Allocator<U>& other) noexcept
      : arena_(other.arena_) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr Allocator(google::protobuf::Arena* ABSL_NULLABLE arena) noexcept
      : arena_(arena) {}

  template <typename U>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr Allocator(
      [[maybe_unused]] const NewDeleteAllocator<U>& other) noexcept
      : arena_(nullptr) {}

  template <typename U>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr Allocator(const ArenaAllocator<U>& other) noexcept
      : arena_(other.arena()) {}

  constexpr google::protobuf::Arena* ABSL_NULLABLE arena() const noexcept {
    return arena_;
  }

  // Allocates at least `nbytes` bytes with a minimum alignment of `alignment`
  // from the underlying memory resource. When the underlying memory resource is
  // `operator new`, `deallocate_bytes` must be called at some point, otherwise
  // calling `deallocate_bytes` is optional. The caller must not pass an object
  // constructed in the return memory to `delete_object`, doing so is undefined
  // behavior.
  ABSL_MUST_USE_RESULT void* allocate_bytes(
      size_type nbytes, size_type alignment = alignof(std::max_align_t)) {
    return arena() != nullptr
               ? ArenaAllocator<void>(arena()).allocate_bytes(nbytes, alignment)
               : NewDeleteAllocator<void>().allocate_bytes(nbytes, alignment);
  }

  // Deallocates memory previously returned by `allocate_bytes`.
  void deallocate_bytes(
      void* p, size_type nbytes,
      size_type alignment = alignof(std::max_align_t)) noexcept {
    arena() != nullptr
        ? ArenaAllocator<void>(arena()).deallocate_bytes(p, nbytes, alignment)
        : NewDeleteAllocator<void>().deallocate_bytes(p, nbytes, alignment);
  }

  template <typename T>
  ABSL_MUST_USE_RESULT T* allocate_object(size_type n = 1) {
    return arena() != nullptr
               ? ArenaAllocator<void>(arena()).allocate_object<T>(n)
               : NewDeleteAllocator<void>().allocate_object<T>(n);
  }

  template <typename T>
  void deallocate_object(T* p, size_type n = 1) {
    arena() != nullptr ? ArenaAllocator<void>(arena()).deallocate_object(p, n)
                       : NewDeleteAllocator<void>().deallocate_object(p, n);
  }

  // Allocates memory suitable for an object of type `T` and constructs the
  // object by forwarding the provided arguments. If the underlying memory
  // resource is `operator new` is false, `delete_object` must eventually be
  // called.
  template <typename T, typename... Args>
  ABSL_MUST_USE_RESULT T* new_object(Args&&... args) {
    return arena() != nullptr ? ArenaAllocator<void>(arena()).new_object<T>(
                                    std::forward<Args>(args)...)
                              : NewDeleteAllocator<void>().new_object<T>(
                                    std::forward<Args>(args)...);
  }

  // Destructs the object of type `T` located at address `p` and deallocates the
  // memory, `p` must have been previously returned by `new_object`.
  template <typename T>
  void delete_object(T* p) noexcept {
    arena() != nullptr ? ArenaAllocator<void>(arena()).delete_object(p)
                       : NewDeleteAllocator<void>().delete_object(p);
  }

  void delete_object(std::nullptr_t) = delete;

 private:
  template <typename U>
  friend class Allocator;

  google::protobuf::Arena* ABSL_NULLABLE arena_;
};

// `Allocator<T>` is an extension of `Allocator<>` which adheres to the named
// C++ requirements for `Allocator`, allowing it to be used in places which
// accept custom STL allocators.
template <typename T>
class Allocator : public Allocator<void> {
 public:
  static_assert(!std::is_const_v<T>, "T must not be const qualified");
  static_assert(!std::is_volatile_v<T>, "T must not be volatile qualified");
  static_assert(std::is_object_v<T>, "T must be an object type");

  using value_type = T;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using reference = value_type&;
  using const_reference = const value_type&;

  using Allocator<void>::Allocator;

  template <typename U, typename = std::enable_if_t<!std::is_same_v<U, T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr Allocator(const Allocator<U>& other) noexcept
      : Allocator(other.arena_) {}

  pointer allocate(size_type n, const void* /*hint*/ = nullptr) {
    return arena() != nullptr ? ArenaAllocator<T>(arena()).allocate(n)
                              : NewDeleteAllocator<T>().allocate(n);
  }

#if defined(__cpp_lib_allocate_at_least) && \
    __cpp_lib_allocate_at_least >= 202302L
  std::allocation_result<pointer, size_type> allocate_at_least(size_type n) {
    return arena() != nullptr ? ArenaAllocator<T>(arena()).allocate_at_least(n)
                              : NewDeleteAllocator<T>().allocate_at_least(n);
  }
#endif

  void deallocate(pointer p, size_type n) noexcept {
    arena() != nullptr ? ArenaAllocator<T>(arena()).deallocate(p, n)
                       : NewDeleteAllocator<T>().deallocate(p, n);
  }

  template <typename U, typename... Args>
  void construct(U* p, Args&&... args) {
    arena() != nullptr
        ? ArenaAllocator<T>(arena()).construct(p, std::forward<Args>(args)...)
        : NewDeleteAllocator<T>().construct(p, std::forward<Args>(args)...);
  }

  template <typename U>
  void destroy(U* p) noexcept {
    arena() != nullptr ? ArenaAllocator<T>(arena()).destroy(p)
                       : NewDeleteAllocator<T>().destroy(p);
  }
};

template <typename T, typename U>
inline bool operator==(Allocator<T> lhs, Allocator<U> rhs) noexcept {
  return lhs.arena() == rhs.arena();
}

template <typename T, typename U>
inline bool operator!=(Allocator<T> lhs, Allocator<U> rhs) noexcept {
  return !operator==(lhs, rhs);
}

Allocator(google::protobuf::Arena* ABSL_NULLABLE) -> Allocator<void>;
template <typename T>
Allocator(const Allocator<T>&) -> Allocator<T>;
template <typename T>
Allocator(const NewDeleteAllocator<T>&) -> Allocator<T>;
template <typename T>
Allocator(const ArenaAllocator<T>&) -> Allocator<T>;

template <typename T>
inline NewDeleteAllocator<T> NewDeleteAllocatorFor() noexcept {
  static_assert(!std::is_void_v<T>);
  return NewDeleteAllocator<T>();
}

template <typename T>
inline Allocator<T> ArenaAllocatorFor(
    google::protobuf::Arena* ABSL_NONNULL arena) noexcept {
  static_assert(!std::is_void_v<T>);
  ABSL_DCHECK(arena != nullptr);
  return Allocator<T>(arena);
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_ALLOCATOR_H_
