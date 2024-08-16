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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "common/type_kind.h"
#include "common/types/any_type.h"   // IWYU pragma: export
#include "common/types/bool_type.h"  // IWYU pragma: export
#include "common/types/bool_wrapper_type.h"  // IWYU pragma: export
#include "common/types/bytes_type.h"  // IWYU pragma: export
#include "common/types/bytes_wrapper_type.h"  // IWYU pragma: export
#include "common/types/double_type.h"  // IWYU pragma: export
#include "common/types/double_wrapper_type.h"  // IWYU pragma: export
#include "common/types/duration_type.h"  // IWYU pragma: export
#include "common/types/dyn_type.h"    // IWYU pragma: export
#include "common/types/enum_type.h"   // IWYU pragma: export
#include "common/types/error_type.h"  // IWYU pragma: export
#include "common/types/function_type.h"  // IWYU pragma: export
#include "common/types/int_type.h"  // IWYU pragma: export
#include "common/types/int_wrapper_type.h"  // IWYU pragma: export
#include "common/types/list_type.h"  // IWYU pragma: export
#include "common/types/map_type.h"   // IWYU pragma: export
#include "common/types/message_type.h"  // IWYU pragma: export
#include "common/types/null_type.h"  // IWYU pragma: export
#include "common/types/opaque_type.h"  // IWYU pragma: export
#include "common/types/optional_type.h"  // IWYU pragma: export
#include "common/types/string_type.h"  // IWYU pragma: export
#include "common/types/string_wrapper_type.h"  // IWYU pragma: export
#include "common/types/struct_type.h"  // IWYU pragma: export
#include "common/types/timestamp_type.h"  // IWYU pragma: export
#include "common/types/type_param_type.h"  // IWYU pragma: export
#include "common/types/type_type.h"  // IWYU pragma: export
#include "common/types/types.h"
#include "common/types/uint_type.h"  // IWYU pragma: export
#include "common/types/uint_wrapper_type.h"  // IWYU pragma: export
#include "common/types/unknown_type.h"  // IWYU pragma: export
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"

namespace cel {

class Type;

// `Type` is a composition type which encompasses all types supported by the
// Common Expression Language. When default constructed, `Type` is in a
// known but invalid state. Any attempt to use it from then on, without
// assigning another type, is undefined behavior. In debug builds, we do our
// best to fail.
//
// The data underlying `Type` is either static or owned by `google::protobuf::Arena`. As
// such, care must be taken to ensure types remain valid throughout their use.
class Type final {
 public:
  // Returns an appropriate `Type` for the dynamic protobuf message. For well
  // known message types, the appropriate `Type` is returned. All others return
  // `MessageType`.
  static Type Message(absl::Nonnull<const google::protobuf::Descriptor*> descriptor
                          ABSL_ATTRIBUTE_LIFETIME_BOUND);

  // Returns an appropriate `Type` for the dynamic protobuf enum. For well
  // known enum types, the appropriate `Type` is returned. All others return
  // `EnumType`.
  static Type Enum(absl::Nonnull<const google::protobuf::EnumDescriptor*> descriptor
                       ABSL_ATTRIBUTE_LIFETIME_BOUND);

  // The default constructor results in Type being DynType.
  Type() = default;
  Type(const Type&) = default;
  Type(Type&&) = default;
  Type& operator=(const Type&) = default;
  Type& operator=(Type&&) = default;

  template <typename T,
            typename = std::enable_if_t<common_internal::IsTypeAlternativeV<
                absl::remove_reference_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr Type(T&& alternative) noexcept
      : variant_(absl::in_place_type<absl::remove_cvref_t<T>>,
                 std::forward<T>(alternative)) {}

  template <typename T,
            typename = std::enable_if_t<common_internal::IsTypeAlternativeV<
                absl::remove_reference_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Type& operator=(T&& type) noexcept {
    variant_.emplace<absl::remove_cvref_t<T>>(std::forward<T>(type));
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Type(StructType alternative) : variant_(alternative.ToTypeVariant()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Type& operator=(StructType alternative) {
    variant_ = alternative.ToTypeVariant();
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Type(OptionalType alternative) : Type(OpaqueType(std::move(alternative))) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Type& operator=(OptionalType alternative) {
    return *this = OpaqueType(std::move(alternative));
  }

  TypeKind kind() const;

  absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  std::string DebugString() const;

  absl::Span<const Type> parameters() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  friend void swap(Type& lhs, Type& rhs) noexcept {
    lhs.variant_.swap(rhs.variant_);
  }

  template <typename H>
  friend H AbslHashValue(H state, const Type& type) {
    return H::combine(
        absl::visit(
            [state = std::move(state)](const auto& alternative) mutable -> H {
              return H::combine(std::move(state), alternative.kind(),
                                alternative);
            },
            type.variant_),
        type.variant_.index());
  }

  friend bool operator==(const Type& lhs, const Type& rhs);

  friend std::ostream& operator<<(std::ostream& out, const Type& type) {
    return absl::visit(
        [&out](const auto& alternative) -> std::ostream& {
          return out << alternative;
        },
        type.variant_);
  }

  bool IsAny() const { return absl::holds_alternative<AnyType>(variant_); }

  bool IsBool() const { return absl::holds_alternative<BoolType>(variant_); }

  bool IsBoolWrapper() const {
    return absl::holds_alternative<BoolWrapperType>(variant_);
  }

  bool IsBytes() const { return absl::holds_alternative<BytesType>(variant_); }

  bool IsBytesWrapper() const {
    return absl::holds_alternative<BytesWrapperType>(variant_);
  }

  bool IsDouble() const {
    return absl::holds_alternative<DoubleType>(variant_);
  }

  bool IsDoubleWrapper() const {
    return absl::holds_alternative<DoubleWrapperType>(variant_);
  }

  bool IsDuration() const {
    return absl::holds_alternative<DurationType>(variant_);
  }

  bool IsDyn() const { return absl::holds_alternative<DynType>(variant_); }

  bool IsEnum() const { return absl::holds_alternative<EnumType>(variant_); }

  bool IsError() const { return absl::holds_alternative<ErrorType>(variant_); }

  bool IsFunction() const {
    return absl::holds_alternative<FunctionType>(variant_);
  }

  bool IsInt() const { return absl::holds_alternative<IntType>(variant_); }

  bool IsIntWrapper() const {
    return absl::holds_alternative<IntWrapperType>(variant_);
  }

  bool IsList() const { return absl::holds_alternative<ListType>(variant_); }

  bool IsMap() const { return absl::holds_alternative<MapType>(variant_); }

  bool IsMessage() const {
    return absl::holds_alternative<MessageType>(variant_);
  }

  bool IsNull() const { return absl::holds_alternative<NullType>(variant_); }

  bool IsOpaque() const {
    return absl::holds_alternative<OpaqueType>(variant_);
  }

  bool IsOptional() const;

  bool IsString() const {
    return absl::holds_alternative<StringType>(variant_);
  }

  bool IsStringWrapper() const {
    return absl::holds_alternative<StringWrapperType>(variant_);
  }

  bool IsStruct() const {
    return absl::holds_alternative<common_internal::BasicStructType>(
               variant_) ||
           absl::holds_alternative<MessageType>(variant_);
  }

  bool IsTimestamp() const {
    return absl::holds_alternative<TimestampType>(variant_);
  }

  bool IsTypeParam() const {
    return absl::holds_alternative<TypeParamType>(variant_);
  }

  bool IsType() const { return absl::holds_alternative<TypeType>(variant_); }

  bool IsUint() const { return absl::holds_alternative<UintType>(variant_); }

  bool IsUintWrapper() const {
    return absl::holds_alternative<UintWrapperType>(variant_);
  }

  bool IsUnknown() const {
    return absl::holds_alternative<UnknownType>(variant_);
  }

  bool IsWrapper() const {
    return IsBoolWrapper() || IsIntWrapper() || IsUintWrapper() ||
           IsDoubleWrapper() || IsBytesWrapper() || IsStringWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<AnyType, T>, bool> Is() const {
    return IsAny();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<BoolType, T>, bool> Is() const {
    return IsBool();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<BoolWrapperType, T>, bool> Is() const {
    return IsBoolWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<BytesType, T>, bool> Is() const {
    return IsBytes();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<BytesWrapperType, T>, bool> Is() const {
    return IsBytesWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleType, T>, bool> Is() const {
    return IsDouble();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleWrapperType, T>, bool> Is() const {
    return IsDoubleWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<DurationType, T>, bool> Is() const {
    return IsDuration();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<DynType, T>, bool> Is() const {
    return IsDyn();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<EnumType, T>, bool> Is() const {
    return IsEnum();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<ErrorType, T>, bool> Is() const {
    return IsError();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<FunctionType, T>, bool> Is() const {
    return IsFunction();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<IntType, T>, bool> Is() const {
    return IsInt();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<IntWrapperType, T>, bool> Is() const {
    return IsIntWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<ListType, T>, bool> Is() const {
    return IsList();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<MapType, T>, bool> Is() const {
    return IsMap();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<MessageType, T>, bool> Is() const {
    return IsMessage();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<NullType, T>, bool> Is() const {
    return IsNull();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<OpaqueType, T>, bool> Is() const {
    return IsOpaque();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalType, T>, bool> Is() const {
    return IsOptional();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<StringType, T>, bool> Is() const {
    return IsString();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<StringWrapperType, T>, bool> Is() const {
    return IsStringWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<StructType, T>, bool> Is() const {
    return IsStruct();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampType, T>, bool> Is() const {
    return IsTimestamp();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<TypeParamType, T>, bool> Is() const {
    return IsTypeParam();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<TypeType, T>, bool> Is() const {
    return IsType();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<UintType, T>, bool> Is() const {
    return IsUint();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<UintWrapperType, T>, bool> Is() const {
    return IsUintWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<UnknownType, T>, bool> Is() const {
    return IsUnknown();
  }

  absl::optional<AnyType> AsAny() const;

  absl::optional<BoolType> AsBool() const;

  absl::optional<BoolWrapperType> AsBoolWrapper() const;

  absl::optional<BytesType> AsBytes() const;

  absl::optional<BytesWrapperType> AsBytesWrapper() const;

  absl::optional<DoubleType> AsDouble() const;

  absl::optional<DoubleWrapperType> AsDoubleWrapper() const;

  absl::optional<DurationType> AsDuration() const;

  absl::optional<DynType> AsDyn() const;

  absl::optional<EnumType> AsEnum() const;

  absl::optional<ErrorType> AsError() const;

  absl::optional<FunctionType> AsFunction() const;

  absl::optional<IntType> AsInt() const;

  absl::optional<IntWrapperType> AsIntWrapper() const;

  absl::optional<ListType> AsList() const;

  absl::optional<MapType> AsMap() const;

  // AsMessage performs a checked cast, returning `MessageType` if this type is
  // both a struct and a message or `absl::nullopt` otherwise. If you have
  // already called `IsMessage()` it is more performant to perform to do
  // `static_cast<MessageType>(type)`.
  absl::optional<MessageType> AsMessage() const;

  absl::optional<NullType> AsNull() const;

  absl::optional<OpaqueType> AsOpaque() const;

  absl::optional<OptionalType> AsOptional() const;

  absl::optional<StringType> AsString() const;

  absl::optional<StringWrapperType> AsStringWrapper() const;

  // AsStruct performs a checked cast, returning `StructType` if this type is a
  // struct or `absl::nullopt` otherwise. If you have already called
  // `IsStruct()` it is more performant to perform to do
  // `static_cast<StructType>(type)`.
  absl::optional<StructType> AsStruct() const;

  absl::optional<TimestampType> AsTimestamp() const;

  absl::optional<TypeParamType> AsTypeParam() const;

  absl::optional<TypeType> AsType() const;

  absl::optional<UintType> AsUint() const;

  absl::optional<UintWrapperType> AsUintWrapper() const;

  absl::optional<UnknownType> AsUnknown() const;

  template <typename T>
  std::enable_if_t<std::is_same_v<AnyType, T>, absl::optional<AnyType>> As()
      const {
    return AsAny();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<BoolType, T>, absl::optional<BoolType>> As()
      const {
    return AsBool();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<BoolWrapperType, T>,
                   absl::optional<BoolWrapperType>>
  As() const {
    return AsBoolWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<BytesType, T>, absl::optional<BytesType>> As()
      const {
    return AsBytes();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<BytesWrapperType, T>,
                   absl::optional<BytesWrapperType>>
  As() const {
    return AsBytesWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleType, T>, absl::optional<DoubleType>>
  As() const {
    return AsDouble();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleWrapperType, T>,
                   absl::optional<DoubleWrapperType>>
  As() const {
    return AsDoubleWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<DurationType, T>,
                   absl::optional<DurationType>>
  As() const {
    return AsDuration();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<DynType, T>, absl::optional<DynType>> As()
      const {
    return AsDyn();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<EnumType, T>, absl::optional<EnumType>> As()
      const {
    return AsEnum();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<ErrorType, T>, absl::optional<ErrorType>> As()
      const {
    return AsError();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<FunctionType, T>,
                   absl::optional<FunctionType>>
  As() const {
    return AsFunction();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<IntType, T>, absl::optional<IntType>> As()
      const {
    return AsInt();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<IntWrapperType, T>,
                   absl::optional<IntWrapperType>>
  As() const {
    return AsIntWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<ListType, T>, absl::optional<ListType>> As()
      const {
    return AsList();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<MapType, T>, absl::optional<MapType>> As()
      const {
    return AsMap();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<MessageType, T>, absl::optional<MessageType>>
  As() const {
    return AsMessage();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<NullType, T>, absl::optional<NullType>> As()
      const {
    return AsNull();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<OpaqueType, T>, absl::optional<OpaqueType>>
  As() const {
    return AsOpaque();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalType, T>,
                   absl::optional<OptionalType>>
  As() const {
    return AsOptional();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<StringType, T>, absl::optional<StringType>>
  As() const {
    return AsString();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<StringWrapperType, T>,
                   absl::optional<StringWrapperType>>
  As() const {
    return AsStringWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<StructType, T>, absl::optional<StructType>>
  As() const {
    return AsStruct();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampType, T>,
                   absl::optional<TimestampType>>
  As() const {
    return AsTimestamp();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<TypeParamType, T>,
                   absl::optional<TypeParamType>>
  As() const {
    return AsTypeParam();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<TypeType, T>, absl::optional<TypeType>> As()
      const {
    return AsType();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<UintType, T>, absl::optional<UintType>> As()
      const {
    return AsUint();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<UintWrapperType, T>,
                   absl::optional<UintWrapperType>>
  As() const {
    return AsUintWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<UnknownType, T>, absl::optional<UnknownType>>
  As() const {
    return AsUnknown();
  }

  explicit operator AnyType() const;

  explicit operator BoolType() const;

  explicit operator BoolWrapperType() const;

  explicit operator BytesType() const;

  explicit operator BytesWrapperType() const;

  explicit operator DoubleType() const;

  explicit operator DoubleWrapperType() const;

  explicit operator DurationType() const;

  explicit operator DynType() const;

  explicit operator EnumType() const;

  explicit operator ErrorType() const;

  explicit operator FunctionType() const;

  explicit operator IntType() const;

  explicit operator IntWrapperType() const;

  explicit operator ListType() const;

  explicit operator MapType() const;

  explicit operator MessageType() const;

  explicit operator NullType() const;

  explicit operator OpaqueType() const;

  explicit operator OptionalType() const;

  explicit operator StringType() const;

  explicit operator StringWrapperType() const;

  explicit operator StructType() const;

  explicit operator TimestampType() const;

  explicit operator TypeParamType() const;

  explicit operator TypeType() const;

  explicit operator UintType() const;

  explicit operator UintWrapperType() const;

  explicit operator UnknownType() const;

 private:
  friend class StructType;
  friend class MessageType;
  friend class common_internal::BasicStructType;

  common_internal::StructTypeVariant ToStructTypeVariant() const;

  common_internal::TypeVariant variant_;
};

inline bool operator!=(const Type& lhs, const Type& rhs) {
  return !operator==(lhs, rhs);
}

inline Type JsonType() { return DynType(); }

// Statically assert some expectations.
static_assert(std::is_default_constructible_v<Type>);
static_assert(std::is_copy_constructible_v<Type>);
static_assert(std::is_copy_assignable_v<Type>);
static_assert(std::is_nothrow_move_constructible_v<Type>);
static_assert(std::is_nothrow_move_assignable_v<Type>);
static_assert(std::is_nothrow_swappable_v<Type>);

struct StructTypeField {
  std::string name;
  Type type;
  // The field number, if less than or equal to 0 it is not available.
  int64_t number = 0;
};

// Now that Type and TypeView are complete, we can define various parts of list,
// map, opaque, and struct which depend on Type and TypeView.

namespace common_internal {

struct ListTypeData final {
  static absl::Nonnull<ListTypeData*> Create(
      absl::Nonnull<google::protobuf::Arena*> arena, const Type& element);

  ListTypeData() = default;
  ListTypeData(const ListTypeData&) = delete;
  ListTypeData(ListTypeData&&) = delete;
  ListTypeData& operator=(const ListTypeData&) = delete;
  ListTypeData& operator=(ListTypeData&&) = delete;

  Type element = DynType();

 private:
  explicit ListTypeData(const Type& element);
};

struct MapTypeData final {
  static absl::Nonnull<MapTypeData*> Create(absl::Nonnull<google::protobuf::Arena*> arena,
                                            const Type& key, const Type& value);

  Type key_and_value[2];
};

struct FunctionTypeData final {
  static absl::Nonnull<FunctionTypeData*> Create(
      absl::Nonnull<google::protobuf::Arena*> arena, const Type& result,
      absl::Span<const Type> args);

  FunctionTypeData() = delete;
  FunctionTypeData(const FunctionTypeData&) = delete;
  FunctionTypeData(FunctionTypeData&&) = delete;
  FunctionTypeData& operator=(const FunctionTypeData&) = delete;
  FunctionTypeData& operator=(FunctionTypeData&&) = delete;

  const size_t args_size;
  // Flexible array, has `args_size` elements, with the first element being the
  // return type. FunctionTypeData has a variable length size, which includes
  // this flexible array.
  Type args[];

 private:
  FunctionTypeData(const Type& result, absl::Span<const Type> args);
};

struct OpaqueTypeData final {
  static absl::Nonnull<OpaqueTypeData*> Create(
      absl::Nonnull<google::protobuf::Arena*> arena, absl::string_view name,
      absl::Span<const Type> parameters);

  OpaqueTypeData() = delete;
  OpaqueTypeData(const OpaqueTypeData&) = delete;
  OpaqueTypeData(OpaqueTypeData&&) = delete;
  OpaqueTypeData& operator=(const OpaqueTypeData&) = delete;
  OpaqueTypeData& operator=(OpaqueTypeData&&) = delete;

  const absl::string_view name;
  const size_t parameters_size;
  // Flexible array, has `parameters_size` elements. OpaqueTypeData has a
  // variable length size, which includes this flexible array.
  Type parameters[];

 private:
  OpaqueTypeData(absl::string_view name, absl::Span<const Type> parameters);
};

}  // namespace common_internal

inline bool operator==(const ListType& lhs, const ListType& rhs) {
  return &lhs == &rhs || lhs.element() == rhs.element();
}

template <typename H>
inline H AbslHashValue(H state, const ListType& type) {
  return H::combine(std::move(state), type.element());
}

inline bool operator==(const MapType& lhs, const MapType& rhs) {
  return &lhs == &rhs || (lhs.key() == rhs.key() && lhs.value() == rhs.value());
}

template <typename H>
inline H AbslHashValue(H state, const MapType& type) {
  return H::combine(std::move(state), type.key(), type.value());
}

inline bool operator==(const OpaqueType& lhs, const OpaqueType& rhs) {
  return lhs.name() == rhs.name() &&
         absl::c_equal(lhs.parameters(), rhs.parameters());
}

template <typename H>
inline H AbslHashValue(H state, const OpaqueType& type) {
  state = H::combine(std::move(state), type.name());
  auto parameters = type.parameters();
  for (const auto& parameter : parameters) {
    state = H::combine(std::move(state), parameter);
  }
  return std::move(state);
}

inline bool operator==(const FunctionType& lhs, const FunctionType& rhs) {
  return lhs.result() == rhs.result() && absl::c_equal(lhs.args(), rhs.args());
}

template <typename H>
inline H AbslHashValue(H state, const FunctionType& type) {
  state = H::combine(std::move(state), type.result());
  auto args = type.args();
  for (const auto& arg : args) {
    state = H::combine(std::move(state), arg);
  }
  return std::move(state);
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_H_
