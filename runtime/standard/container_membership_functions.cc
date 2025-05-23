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

#include "runtime/standard/container_membership_functions.h"

#include <array>
#include <cstdint>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/builtins.h"
#include "base/function_adapter.h"
#include "common/value.h"
#include "internal/number.h"
#include "internal/status_macros.h"
#include "runtime/function_registry.h"
#include "runtime/register_function_helper.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {
namespace {

using ::cel::internal::Number;

static constexpr std::array<absl::string_view, 3> in_operators = {
    cel::builtin::kIn,            // @in for map and list types.
    cel::builtin::kInFunction,    // deprecated in() -- for backwards compat
    cel::builtin::kInDeprecated,  // deprecated _in_ -- for backwards compat
};

template <class T>
bool ValueEquals(const Value& value, T other);

template <>
bool ValueEquals(const Value& value, bool other) {
  if (auto bool_value = As<BoolValue>(value); bool_value) {
    return bool_value->NativeValue() == other;
  }
  return false;
}

template <>
bool ValueEquals(const Value& value, int64_t other) {
  if (auto int_value = As<IntValue>(value); int_value) {
    return int_value->NativeValue() == other;
  }
  return false;
}

template <>
bool ValueEquals(const Value& value, uint64_t other) {
  if (auto uint_value = As<UintValue>(value); uint_value) {
    return uint_value->NativeValue() == other;
  }
  return false;
}

template <>
bool ValueEquals(const Value& value, double other) {
  if (auto double_value = As<DoubleValue>(value); double_value) {
    return double_value->NativeValue() == other;
  }
  return false;
}

template <>
bool ValueEquals(const Value& value, const StringValue& other) {
  if (auto string_value = As<StringValue>(value); string_value) {
    return string_value->Equals(other);
  }
  return false;
}

template <>
bool ValueEquals(const Value& value, const BytesValue& other) {
  if (auto bytes_value = As<BytesValue>(value); bytes_value) {
    return bytes_value->Equals(other);
  }
  return false;
}

// Template function implementing CEL in() function
template <typename T>
absl::StatusOr<bool> In(
    T value, const ListValue& list,
    const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
    google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
    google::protobuf::Arena* ABSL_NONNULL arena) {
  CEL_ASSIGN_OR_RETURN(auto size, list.Size());
  Value element;
  for (int i = 0; i < size; i++) {
    CEL_RETURN_IF_ERROR(
        list.Get(i, descriptor_pool, message_factory, arena, &element));
    if (ValueEquals<T>(element, value)) {
      return true;
    }
  }

  return false;
}

// Implementation for @in operator using heterogeneous equality.
absl::StatusOr<Value> HeterogeneousEqualityIn(
    const Value& value, const ListValue& list,
    const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
    google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
    google::protobuf::Arena* ABSL_NONNULL arena) {
  return list.Contains(value, descriptor_pool, message_factory, arena);
}

absl::Status RegisterListMembershipFunctions(FunctionRegistry& registry,
                                             const RuntimeOptions& options) {
  for (absl::string_view op : in_operators) {
    if (options.enable_heterogeneous_equality) {
      CEL_RETURN_IF_ERROR(
          (RegisterHelper<BinaryFunctionAdapter<
               absl::StatusOr<Value>, const Value&, const ListValue&>>::
               RegisterGlobalOverload(op, &HeterogeneousEqualityIn, registry)));
    } else {
      CEL_RETURN_IF_ERROR(
          (RegisterHelper<BinaryFunctionAdapter<absl::StatusOr<bool>, bool,
                                                const ListValue&>>::
               RegisterGlobalOverload(op, In<bool>, registry)));
      CEL_RETURN_IF_ERROR(
          (RegisterHelper<BinaryFunctionAdapter<absl::StatusOr<bool>, int64_t,
                                                const ListValue&>>::
               RegisterGlobalOverload(op, In<int64_t>, registry)));
      CEL_RETURN_IF_ERROR(
          (RegisterHelper<BinaryFunctionAdapter<absl::StatusOr<bool>, uint64_t,
                                                const ListValue&>>::
               RegisterGlobalOverload(op, In<uint64_t>, registry)));
      CEL_RETURN_IF_ERROR(
          (RegisterHelper<BinaryFunctionAdapter<absl::StatusOr<bool>, double,
                                                const ListValue&>>::
               RegisterGlobalOverload(op, In<double>, registry)));
      CEL_RETURN_IF_ERROR(
          (RegisterHelper<BinaryFunctionAdapter<
               absl::StatusOr<bool>, const StringValue&, const ListValue&>>::
               RegisterGlobalOverload(op, In<const StringValue&>, registry)));
      CEL_RETURN_IF_ERROR(
          (RegisterHelper<BinaryFunctionAdapter<
               absl::StatusOr<bool>, const BytesValue&, const ListValue&>>::
               RegisterGlobalOverload(op, In<const BytesValue&>, registry)));
    }
  }
  return absl::OkStatus();
}

absl::Status RegisterMapMembershipFunctions(FunctionRegistry& registry,
                                            const RuntimeOptions& options) {
  const bool enable_heterogeneous_equality =
      options.enable_heterogeneous_equality;

  auto boolKeyInSet =
      [enable_heterogeneous_equality](
          bool key, const MapValue& map_value,
          const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
          google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
          google::protobuf::Arena* ABSL_NONNULL arena) -> absl::StatusOr<Value> {
    auto result =
        map_value.Has(BoolValue(key), descriptor_pool, message_factory, arena);
    if (result.ok()) {
      return std::move(*result);
    }
    if (enable_heterogeneous_equality) {
      return BoolValue(false);
    }
    return ErrorValue(result.status());
  };

  auto intKeyInSet =
      [enable_heterogeneous_equality](
          int64_t key, const MapValue& map_value,
          const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
          google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
          google::protobuf::Arena* ABSL_NONNULL arena) -> absl::StatusOr<Value> {
    auto result =
        map_value.Has(IntValue(key), descriptor_pool, message_factory, arena);
    if (enable_heterogeneous_equality) {
      if (result.ok() && result->IsTrue()) {
        return std::move(*result);
      }
      Number number = Number::FromInt64(key);
      if (number.LosslessConvertibleToUint()) {
        const auto& result =
            map_value.Has(UintValue(number.AsUint()), descriptor_pool,
                          message_factory, arena);
        if (result.ok() && result->IsTrue()) {
          return std::move(*result);
        }
      }
      return BoolValue(false);
    }
    if (!result.ok()) {
      return ErrorValue(result.status());
    }
    return std::move(*result);
  };

  auto stringKeyInSet =
      [enable_heterogeneous_equality](
          const StringValue& key, const MapValue& map_value,
          const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
          google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
          google::protobuf::Arena* ABSL_NONNULL arena) -> absl::StatusOr<Value> {
    auto result = map_value.Has(key, descriptor_pool, message_factory, arena);
    if (result.ok()) {
      return std::move(*result);
    }
    if (enable_heterogeneous_equality) {
      return BoolValue(false);
    }
    return ErrorValue(result.status());
  };

  auto uintKeyInSet =
      [enable_heterogeneous_equality](
          uint64_t key, const MapValue& map_value,
          const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
          google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
          google::protobuf::Arena* ABSL_NONNULL arena) -> absl::StatusOr<Value> {
    const auto& result =
        map_value.Has(UintValue(key), descriptor_pool, message_factory, arena);
    if (enable_heterogeneous_equality) {
      if (result.ok() && result->IsTrue()) {
        return std::move(*result);
      }
      Number number = Number::FromUint64(key);
      if (number.LosslessConvertibleToInt()) {
        const auto& result = map_value.Has(
            IntValue(number.AsInt()), descriptor_pool, message_factory, arena);
        if (result.ok() && result->IsTrue()) {
          return std::move(*result);
        }
      }
      return BoolValue(false);
    }
    if (!result.ok()) {
      return ErrorValue(result.status());
    }
    return std::move(*result);
  };

  auto doubleKeyInSet =
      [](double key, const MapValue& map_value,
         const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
         google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
         google::protobuf::Arena* ABSL_NONNULL arena) -> absl::StatusOr<Value> {
    Number number = Number::FromDouble(key);
    if (number.LosslessConvertibleToInt()) {
      const auto& result = map_value.Has(
          IntValue(number.AsInt()), descriptor_pool, message_factory, arena);
      if (result.ok() && result->IsTrue()) {
        return std::move(*result);
      }
    }
    if (number.LosslessConvertibleToUint()) {
      const auto& result = map_value.Has(
          UintValue(number.AsUint()), descriptor_pool, message_factory, arena);
      if (result.ok() && result->IsTrue()) {
        return std::move(*result);
      }
    }
    return BoolValue(false);
  };

  for (auto op : in_operators) {
    auto status = RegisterHelper<BinaryFunctionAdapter<
        absl::StatusOr<Value>, const StringValue&,
        const MapValue&>>::RegisterGlobalOverload(op, stringKeyInSet, registry);
    if (!status.ok()) return status;

    status = RegisterHelper<
        BinaryFunctionAdapter<absl::StatusOr<Value>, bool, const MapValue&>>::
        RegisterGlobalOverload(op, boolKeyInSet, registry);
    if (!status.ok()) return status;

    status = RegisterHelper<BinaryFunctionAdapter<absl::StatusOr<Value>,
                                                  int64_t, const MapValue&>>::
        RegisterGlobalOverload(op, intKeyInSet, registry);
    if (!status.ok()) return status;

    status = RegisterHelper<BinaryFunctionAdapter<absl::StatusOr<Value>,
                                                  uint64_t, const MapValue&>>::
        RegisterGlobalOverload(op, uintKeyInSet, registry);
    if (!status.ok()) return status;

    if (enable_heterogeneous_equality) {
      status = RegisterHelper<BinaryFunctionAdapter<absl::StatusOr<Value>,
                                                    double, const MapValue&>>::
          RegisterGlobalOverload(op, doubleKeyInSet, registry);
      if (!status.ok()) return status;
    }
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status RegisterContainerMembershipFunctions(
    FunctionRegistry& registry, const RuntimeOptions& options) {
  if (options.enable_list_contains) {
    CEL_RETURN_IF_ERROR(RegisterListMembershipFunctions(registry, options));
  }
  return RegisterMapMembershipFunctions(registry, options);
}

}  // namespace cel
