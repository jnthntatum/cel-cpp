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

#include <string>

#include "google/protobuf/duration.pb.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "common/value.h"
#include "internal/status_macros.h"
#include "internal/time.h"
#include "internal/well_known_types.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

namespace {

using ::cel::well_known_types::DurationReflection;
using ::cel::well_known_types::ValueReflection;

std::string DurationDebugString(absl::Duration value) {
  return internal::DebugStringDuration(value);
}

}  // namespace

std::string DurationValue::DebugString() const {
  return DurationDebugString(NativeValue());
}

absl::Status DurationValue::SerializeTo(
    const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
    google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
    google::protobuf::io::ZeroCopyOutputStream* ABSL_NONNULL output) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(output != nullptr);

  google::protobuf::Duration message;
  CEL_RETURN_IF_ERROR(
      DurationReflection::SetFromAbslDuration(&message, NativeValue()));
  if (!message.SerializePartialToZeroCopyStream(output)) {
    return absl::UnknownError(
        absl::StrCat("failed to serialize message: ", message.GetTypeName()));
  }

  return absl::OkStatus();
}

absl::Status DurationValue::ConvertToJson(
    const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
    google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
    google::protobuf::Message* ABSL_NONNULL json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE);

  ValueReflection value_reflection;
  CEL_RETURN_IF_ERROR(value_reflection.Initialize(json->GetDescriptor()));
  value_reflection.SetStringValueFromDuration(json, NativeValue());

  return absl::OkStatus();
}

absl::Status DurationValue::Equal(
    const Value& other,
    const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
    google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
    google::protobuf::Arena* ABSL_NONNULL arena, Value* ABSL_NONNULL result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  if (auto other_value = other.AsDuration(); other_value.has_value()) {
    *result = BoolValue{NativeValue() == other_value->NativeValue()};
    return absl::OkStatus();
  }
  *result = FalseValue();
  return absl::OkStatus();
}

}  // namespace cel
