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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_MEMORY_MANAGER_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_MEMORY_MANAGER_H_

#include <utility>

#include "google/protobuf/arena.h"
#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "base/memory.h"
#include "common/native_type.h"
#include "internal/casts.h"

namespace cel::extensions {

// `ProtoMemoryManager` is an implementation of `ArenaMemoryManager` using
// `google::protobuf::Arena`. All allocations are valid so long as the underlying
// `google::protobuf::Arena` is still alive.
class ProtoMemoryManager final : public ArenaMemoryManager {
 public:
  static bool Is(const MemoryManager& manager) {
    return manager.TypeId() == cel::NativeTypeId::For<ProtoMemoryManager>();
  }

  // Passing a nullptr is highly discouraged, but supported for backwards
  // compatibility. If `arena` is a nullptr, `ProtoMemoryManager` acts like
  // `MemoryManager::Default()` and then must outlive all allocations.
  explicit ProtoMemoryManager(google::protobuf::Arena* arena)
      : ArenaMemoryManager(arena != nullptr), arena_(arena) {}

  ProtoMemoryManager(const ProtoMemoryManager&) = delete;

  ProtoMemoryManager(ProtoMemoryManager&&) = delete;

  ProtoMemoryManager& operator=(const ProtoMemoryManager&) = delete;

  ProtoMemoryManager& operator=(ProtoMemoryManager&&) = delete;

  constexpr google::protobuf::Arena* arena() const { return arena_; }

  // Expose the underlying google::protobuf::Arena on a generic MemoryManager. This may
  // only be called on an instance that is guaranteed to be a
  // ProtoMemoryManager.
  //
  // Note: underlying arena may be null.
  static google::protobuf::Arena* CastToProtoArena(MemoryManager& manager) {
    ABSL_ASSERT(Is(manager));
    return cel::internal::down_cast<ProtoMemoryManager&>(manager).arena();
  }

 private:
  void* Allocate(size_t size, size_t align) override;

  void OwnDestructor(void* pointer, void (*destruct)(void*)) override;

  cel::NativeTypeId TypeId() const override {
    return cel::NativeTypeId::For<ProtoMemoryManager>();
  }

  google::protobuf::Arena* const arena_;
};

// Allocate and construct `T` using the `ProtoMemoryManager` provided as
// `memory_manager`. `memory_manager` must be `ProtoMemoryManager` or behavior
// is undefined. Unlike `MemoryManager::New`, this method supports arena-enabled
// messages.
template <typename T, typename... Args>
ABSL_MUST_USE_RESULT T* NewInProtoArena(MemoryManager& memory_manager,
                                        Args&&... args) {
  return google::protobuf::Arena::Create<T>(
      ProtoMemoryManager::CastToProtoArena(memory_manager),
      std::forward<Args>(args)...);
}

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_MEMORY_MANAGER_H_
