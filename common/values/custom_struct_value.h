// Copyright 2023 Google LLC
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

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_STRUCT_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_STRUCT_VALUE_H_

#include <cstdint>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "common/allocator.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/custom_value_interface.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

class CustomStructValueInterface;
class CustomStructValue;
class Value;
class ValueManager;

class CustomStructValueInterface : public CustomValueInterface {
 public:
  using alternative_type = CustomStructValue;

  static constexpr ValueKind kKind = ValueKind::kStruct;

  ValueKind kind() const final { return kKind; }

  virtual StructType GetRuntimeType() const {
    return common_internal::MakeBasicStructType(GetTypeName());
  }

  using ForEachFieldCallback =
      absl::FunctionRef<absl::StatusOr<bool>(absl::string_view, const Value&)>;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;

  virtual bool IsZeroValue() const = 0;

  virtual absl::Status GetFieldByName(
      ValueManager& value_manager, absl::string_view name, Value& result,
      ProtoWrapperTypeOptions unboxing_options) const = 0;

  virtual absl::Status GetFieldByNumber(
      ValueManager& value_manager, int64_t number, Value& result,
      ProtoWrapperTypeOptions unboxing_options) const = 0;

  virtual absl::StatusOr<bool> HasFieldByName(absl::string_view name) const = 0;

  virtual absl::StatusOr<bool> HasFieldByNumber(int64_t number) const = 0;

  virtual absl::Status ForEachField(ValueManager& value_manager,
                                    ForEachFieldCallback callback) const = 0;

  virtual absl::StatusOr<int> Qualify(
      ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
      bool presence_test, Value& result) const;

  virtual CustomStructValue Clone(ArenaAllocator<> allocator) const = 0;

 protected:
  virtual absl::Status EqualImpl(ValueManager& value_manager,
                                 const CustomStructValue& other,
                                 Value& result) const;
};

class CustomStructValue {
 public:
  using interface_type = CustomStructValueInterface;

  static constexpr ValueKind kKind = CustomStructValueInterface::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  CustomStructValue(Shared<const CustomStructValueInterface> interface)
      : interface_(std::move(interface)) {}

  CustomStructValue() = default;
  CustomStructValue(const CustomStructValue&) = default;
  CustomStructValue(CustomStructValue&&) = default;
  CustomStructValue& operator=(const CustomStructValue&) = default;
  CustomStructValue& operator=(CustomStructValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  StructType GetRuntimeType() const { return interface_->GetRuntimeType(); }

  absl::string_view GetTypeName() const { return interface_->GetTypeName(); }

  std::string DebugString() const { return interface_->DebugString(); }

  // See Value::SerializeTo().
  absl::Status SerializeTo(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Cord& value) const {
    return interface_->SerializeTo(descriptor_pool, message_factory, value);
  }

  // See Value::ConvertToJson().
  absl::Status ConvertToJson(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Message*> json) const {
    return interface_->ConvertToJson(descriptor_pool, message_factory, json);
  }

  // See Value::ConvertToJsonObject().
  absl::Status ConvertToJsonObject(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Message*> json) const {
    return interface_->ConvertToJsonObject(descriptor_pool, message_factory,
                                           json);
  }

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;

  bool IsZeroValue() const { return interface_->IsZeroValue(); }

  CustomStructValue Clone(Allocator<> allocator) const;

  void swap(CustomStructValue& other) noexcept {
    using std::swap;
    swap(interface_, other.interface_);
  }

  absl::Status GetFieldByName(ValueManager& value_manager,
                              absl::string_view name, Value& result,
                              ProtoWrapperTypeOptions unboxing_options) const;

  absl::Status GetFieldByNumber(ValueManager& value_manager, int64_t number,
                                Value& result,
                                ProtoWrapperTypeOptions unboxing_options) const;

  absl::StatusOr<bool> HasFieldByName(absl::string_view name) const {
    return interface_->HasFieldByName(name);
  }

  absl::StatusOr<bool> HasFieldByNumber(int64_t number) const {
    return interface_->HasFieldByNumber(number);
  }

  using ForEachFieldCallback = CustomStructValueInterface::ForEachFieldCallback;

  absl::Status ForEachField(ValueManager& value_manager,
                            ForEachFieldCallback callback) const;

  absl::StatusOr<int> Qualify(ValueManager& value_manager,
                              absl::Span<const SelectQualifier> qualifiers,
                              bool presence_test, Value& result) const;

  const interface_type& operator*() const { return *interface_; }

  absl::Nonnull<const interface_type*> operator->() const {
    return interface_.operator->();
  }

  explicit operator bool() const { return static_cast<bool>(interface_); }

 private:
  friend struct NativeTypeTraits<CustomStructValue>;

  Shared<const CustomStructValueInterface> interface_;
};

inline void swap(CustomStructValue& lhs, CustomStructValue& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out,
                                const CustomStructValue& value) {
  return out << value.DebugString();
}

template <>
struct NativeTypeTraits<CustomStructValue> final {
  static NativeTypeId Id(const CustomStructValue& type) {
    return NativeTypeId::Of(*type.interface_);
  }

  static bool SkipDestructor(const CustomStructValue& type) {
    return NativeType::SkipDestructor(type.interface_);
  }
};

template <typename T>
struct NativeTypeTraits<
    T, std::enable_if_t<
           std::conjunction_v<std::negation<std::is_same<CustomStructValue, T>>,
                              std::is_base_of<CustomStructValue, T>>>>
    final {
  static NativeTypeId Id(const T& type) {
    return NativeTypeTraits<CustomStructValue>::Id(type);
  }

  static bool SkipDestructor(const T& type) {
    return NativeTypeTraits<CustomStructValue>::SkipDestructor(type);
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_STRUCT_VALUE_H_
