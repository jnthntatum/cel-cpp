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

#include "extensions/protobuf/value.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "base/attribute.h"
#include "base/internal/message_wrapper.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "common/value_testing.h"
#include "eval/public/structs/proto_message_type_adapter.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/proto_matchers.h"
#include "internal/testing.h"
#include "proto/test/v1/proto2/test_all_types.pb.h"
#include "google/protobuf/text_format.h"

namespace cel::extensions {
namespace {

using ::cel::internal::test::EqualsProto;
using ::cel::test::BoolValueIs;
using ::cel::test::BytesValueIs;
using ::cel::test::DoubleValueIs;
using ::cel::test::DurationValueIs;
using ::cel::test::ErrorValueIs;
using ::cel::test::IntValueIs;
using ::cel::test::ListValueIs;
using ::cel::test::MapValueIs;
using ::cel::test::StringValueIs;
using ::cel::test::StructValueFieldHas;
using ::cel::test::StructValueFieldIs;
using ::cel::test::StructValueIs;
using ::cel::test::TimestampValueIs;
using ::cel::test::UintValueIs;
using ::cel::test::ValueKindIs;
using ::google::api::expr::test::v1::proto2::TestAllTypes;
using testing::_;
using testing::AllOf;
using testing::ElementsAre;
using testing::Eq;
using testing::HasSubstr;
using testing::IsTrue;
using testing::Pair;
using testing::UnorderedElementsAre;
using cel::internal::IsOk;
using cel::internal::IsOkAndHolds;
using cel::internal::StatusIs;

template <typename T>
T ParseTextOrDie(absl::string_view text) {
  T proto;
  ABSL_CHECK(google::protobuf::TextFormat::ParseFromString(text, &proto));
  return proto;
}

class ProtoValueTest : public common_internal::ThreadCompatibleValueTest<> {
 protected:
  MemoryManager NewThreadCompatiblePoolingMemoryManager() override {
    return ProtoMemoryManager();
  }
};

class ProtoValueWrapTest : public ProtoValueTest {};

TEST_P(ProtoValueWrapTest, ProtoEnumToValue) {
  ASSERT_OK_AND_ASSIGN(
      auto enum_value,
      ProtoEnumToValue(value_factory(),
                       google::protobuf::NullValue::NULL_VALUE));
  ASSERT_TRUE(InstanceOf<NullValue>(enum_value));
  ASSERT_OK_AND_ASSIGN(enum_value,
                       ProtoEnumToValue(value_factory(), TestAllTypes::BAR));
  ASSERT_TRUE(InstanceOf<IntValue>(enum_value));
  ASSERT_THAT(Cast<IntValue>(enum_value).NativeValue(), Eq(1));
}

TEST_P(ProtoValueWrapTest, ProtoBoolValueToValue) {
  google::protobuf::BoolValue message;
  message.set_value(true);
  EXPECT_THAT(ProtoMessageToValue(value_manager(), message),
              IsOkAndHolds(BoolValueIs(Eq(true))));
  EXPECT_THAT(ProtoMessageToValue(value_manager(), std::move(message)),
              IsOkAndHolds(BoolValueIs(Eq(true))));
}

TEST_P(ProtoValueWrapTest, ProtoInt32ValueToValue) {
  google::protobuf::Int32Value message;
  message.set_value(1);
  EXPECT_THAT(ProtoMessageToValue(value_manager(), message),
              IsOkAndHolds(IntValueIs(Eq(1))));
  EXPECT_THAT(ProtoMessageToValue(value_manager(), std::move(message)),
              IsOkAndHolds(IntValueIs(Eq(1))));
}

TEST_P(ProtoValueWrapTest, ProtoInt64ValueToValue) {
  google::protobuf::Int64Value message;
  message.set_value(1);
  EXPECT_THAT(ProtoMessageToValue(value_manager(), message),
              IsOkAndHolds(IntValueIs(Eq(1))));
  EXPECT_THAT(ProtoMessageToValue(value_manager(), std::move(message)),
              IsOkAndHolds(IntValueIs(Eq(1))));
}

TEST_P(ProtoValueWrapTest, ProtoUInt32ValueToValue) {
  google::protobuf::UInt32Value message;
  message.set_value(1);
  EXPECT_THAT(ProtoMessageToValue(value_manager(), message),
              IsOkAndHolds(UintValueIs(Eq(1))));
  EXPECT_THAT(ProtoMessageToValue(value_manager(), std::move(message)),
              IsOkAndHolds(UintValueIs(Eq(1))));
}

TEST_P(ProtoValueWrapTest, ProtoUInt64ValueToValue) {
  google::protobuf::UInt64Value message;
  message.set_value(1);
  EXPECT_THAT(ProtoMessageToValue(value_manager(), message),
              IsOkAndHolds(UintValueIs(Eq(1))));
  EXPECT_THAT(ProtoMessageToValue(value_manager(), std::move(message)),
              IsOkAndHolds(UintValueIs(Eq(1))));
}

TEST_P(ProtoValueWrapTest, ProtoFloatValueToValue) {
  google::protobuf::FloatValue message;
  message.set_value(1);
  EXPECT_THAT(ProtoMessageToValue(value_manager(), message),
              IsOkAndHolds(DoubleValueIs(Eq(1))));
  EXPECT_THAT(ProtoMessageToValue(value_manager(), std::move(message)),
              IsOkAndHolds(DoubleValueIs(Eq(1))));
}

TEST_P(ProtoValueWrapTest, ProtoDoubleValueToValue) {
  google::protobuf::DoubleValue message;
  message.set_value(1);
  EXPECT_THAT(ProtoMessageToValue(value_manager(), message),
              IsOkAndHolds(DoubleValueIs(Eq(1))));
  EXPECT_THAT(ProtoMessageToValue(value_manager(), std::move(message)),
              IsOkAndHolds(DoubleValueIs(Eq(1))));
}

TEST_P(ProtoValueWrapTest, ProtoBytesValueToValue) {
  google::protobuf::BytesValue message;
  message.set_value("foo");
  EXPECT_THAT(ProtoMessageToValue(value_manager(), message),
              IsOkAndHolds(BytesValueIs(Eq("foo"))));
  EXPECT_THAT(ProtoMessageToValue(value_manager(), std::move(message)),
              IsOkAndHolds(BytesValueIs(Eq("foo"))));
}

TEST_P(ProtoValueWrapTest, ProtoStringValueToValue) {
  google::protobuf::StringValue message;
  message.set_value("foo");
  EXPECT_THAT(ProtoMessageToValue(value_manager(), message),
              IsOkAndHolds(StringValueIs(Eq("foo"))));
  EXPECT_THAT(ProtoMessageToValue(value_manager(), std::move(message)),
              IsOkAndHolds(StringValueIs(Eq("foo"))));
}

TEST_P(ProtoValueWrapTest, ProtoDurationToValue) {
  google::protobuf::Duration message;
  message.set_seconds(1);
  message.set_nanos(1);
  EXPECT_THAT(ProtoMessageToValue(value_manager(), message),
              IsOkAndHolds(DurationValueIs(
                  Eq(absl::Seconds(1) + absl::Nanoseconds(1)))));
  EXPECT_THAT(ProtoMessageToValue(value_manager(), std::move(message)),
              IsOkAndHolds(DurationValueIs(
                  Eq(absl::Seconds(1) + absl::Nanoseconds(1)))));
}

TEST_P(ProtoValueWrapTest, ProtoTimestampToValue) {
  google::protobuf::Timestamp message;
  message.set_seconds(1);
  message.set_nanos(1);
  EXPECT_THAT(
      ProtoMessageToValue(value_manager(), message),
      IsOkAndHolds(TimestampValueIs(
          Eq(absl::UnixEpoch() + absl::Seconds(1) + absl::Nanoseconds(1)))));
  EXPECT_THAT(
      ProtoMessageToValue(value_manager(), std::move(message)),
      IsOkAndHolds(TimestampValueIs(
          Eq(absl::UnixEpoch() + absl::Seconds(1) + absl::Nanoseconds(1)))));
}

TEST_P(ProtoValueWrapTest, ProtoMessageToValue) {
  TestAllTypes message;
  EXPECT_THAT(ProtoMessageToValue(value_manager(), message),
              IsOkAndHolds(ValueKindIs(Eq(ValueKind::kStruct))));
  EXPECT_THAT(ProtoMessageToValue(value_manager(), std::move(message)),
              IsOkAndHolds(ValueKindIs(Eq(ValueKind::kStruct))));
}

TEST_P(ProtoValueWrapTest, GetFieldByName) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoMessageToValue(value_manager(), ParseTextOrDie<TestAllTypes>(
                                               R"pb(single_int32: 1,
                                                    single_int64: 1
                                                    single_uint32: 1
                                                    single_uint64: 1
                                                    single_float: 1
                                                    single_double: 1
                                                    single_bool: true
                                                    single_string: "foo"
                                                    single_bytes: "foo")pb")));
  EXPECT_THAT(value, StructValueIs(StructValueFieldIs(
                         &value_manager(), "single_int32", IntValueIs(Eq(1)))));
  EXPECT_THAT(value,
              StructValueIs(StructValueFieldHas("single_int32", IsTrue())));
  EXPECT_THAT(value, StructValueIs(StructValueFieldIs(
                         &value_manager(), "single_int64", IntValueIs(Eq(1)))));
  EXPECT_THAT(value,
              StructValueIs(StructValueFieldHas("single_int64", IsTrue())));
  EXPECT_THAT(
      value, StructValueIs(StructValueFieldIs(&value_manager(), "single_uint32",
                                              UintValueIs(Eq(1)))));
  EXPECT_THAT(value,
              StructValueIs(StructValueFieldHas("single_uint32", IsTrue())));
  EXPECT_THAT(
      value, StructValueIs(StructValueFieldIs(&value_manager(), "single_uint64",
                                              UintValueIs(Eq(1)))));
  EXPECT_THAT(value,
              StructValueIs(StructValueFieldHas("single_uint64", IsTrue())));
}

TEST_P(ProtoValueWrapTest, GetFieldNoSuchField) {
  ASSERT_OK_AND_ASSIGN(
      auto value, ProtoMessageToValue(
                      value_manager(),
                      ParseTextOrDie<TestAllTypes>(R"pb(single_int32: 1)pb")));
  ASSERT_THAT(value, StructValueIs(_));

  StructValue struct_value = Cast<StructValue>(value);
  EXPECT_THAT(struct_value.GetFieldByName(value_manager(), "does_not_exist"),
              IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kNotFound,
                                                 HasSubstr("no_such_field")))));
}

TEST_P(ProtoValueWrapTest, GetFieldByNumber) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoMessageToValue(value_manager(), ParseTextOrDie<TestAllTypes>(
                                               R"pb(single_int32: 1,
                                                    single_int64: 2
                                                    single_uint32: 3
                                                    single_uint64: 4
                                                    single_float: 1.25
                                                    single_double: 1.5
                                                    single_bool: true
                                                    single_string: "foo"
                                                    single_bytes: "foo")pb")));
  EXPECT_THAT(value, StructValueIs(_));
  StructValue struct_value = Cast<StructValue>(value);

  EXPECT_THAT(struct_value.GetFieldByNumber(
                  value_manager(), TestAllTypes::kSingleInt32FieldNumber),
              IsOkAndHolds(IntValueIs(1)));
  EXPECT_THAT(struct_value.GetFieldByNumber(
                  value_manager(), TestAllTypes::kSingleInt64FieldNumber),
              IsOkAndHolds(IntValueIs(2)));
  EXPECT_THAT(struct_value.GetFieldByNumber(
                  value_manager(), TestAllTypes::kSingleUint32FieldNumber),
              IsOkAndHolds(UintValueIs(3)));
  EXPECT_THAT(struct_value.GetFieldByNumber(
                  value_manager(), TestAllTypes::kSingleUint64FieldNumber),
              IsOkAndHolds(UintValueIs(4)));

  EXPECT_THAT(struct_value.GetFieldByNumber(
                  value_manager(), TestAllTypes::kSingleFloatFieldNumber),
              IsOkAndHolds(DoubleValueIs(1.25)));

  EXPECT_THAT(struct_value.GetFieldByNumber(
                  value_manager(), TestAllTypes::kSingleDoubleFieldNumber),
              IsOkAndHolds(DoubleValueIs(1.5)));

  EXPECT_THAT(struct_value.GetFieldByNumber(
                  value_manager(), TestAllTypes::kSingleBoolFieldNumber),
              IsOkAndHolds(BoolValueIs(true)));

  EXPECT_THAT(struct_value.GetFieldByNumber(
                  value_manager(), TestAllTypes::kSingleStringFieldNumber),
              IsOkAndHolds(StringValueIs("foo")));

  EXPECT_THAT(struct_value.GetFieldByNumber(
                  value_manager(), TestAllTypes::kSingleBytesFieldNumber),
              IsOkAndHolds(BytesValueIs("foo")));
}

TEST_P(ProtoValueWrapTest, GetFieldByNumberNoSuchField) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoMessageToValue(value_manager(), ParseTextOrDie<TestAllTypes>(
                                               R"pb(single_int32: 1,
                                                    single_int64: 2
                                                    single_uint32: 3
                                                    single_uint64: 4
                                                    single_float: 1.25
                                                    single_double: 1.5
                                                    single_bool: true
                                                    single_string: "foo"
                                                    single_bytes: "foo")pb")));
  EXPECT_THAT(value, StructValueIs(_));
  StructValue struct_value = Cast<StructValue>(value);

  EXPECT_THAT(struct_value.GetFieldByNumber(value_manager(), 999),
              IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kNotFound,
                                                 HasSubstr("no_such_field")))));

  // Out of range.
  EXPECT_THAT(struct_value.GetFieldByNumber(value_manager(), 0x1ffffffff),
              IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kNotFound,
                                                 HasSubstr("no_such_field")))));
}

TEST_P(ProtoValueWrapTest, HasFieldByNumber) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoMessageToValue(value_manager(), ParseTextOrDie<TestAllTypes>(
                                               R"pb(single_int32: 1,
                                                    single_int64: 2)pb")));
  EXPECT_THAT(value, StructValueIs(_));
  StructValue struct_value = Cast<StructValue>(value);

  EXPECT_THAT(
      struct_value.HasFieldByNumber(TestAllTypes::kSingleInt32FieldNumber),
      IsOkAndHolds(BoolValue(true)));
  EXPECT_THAT(
      struct_value.HasFieldByNumber(TestAllTypes::kSingleInt64FieldNumber),
      IsOkAndHolds(BoolValue(true)));
  EXPECT_THAT(
      struct_value.HasFieldByNumber(TestAllTypes::kSingleStringFieldNumber),
      IsOkAndHolds(BoolValue(false)));
  EXPECT_THAT(
      struct_value.HasFieldByNumber(TestAllTypes::kSingleBytesFieldNumber),
      IsOkAndHolds(BoolValue(false)));
}

TEST_P(ProtoValueWrapTest, HasFieldByNumberNoSuchField) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoMessageToValue(value_manager(), ParseTextOrDie<TestAllTypes>(
                                               R"pb(single_int32: 1,
                                                    single_int64: 2)pb")));
  EXPECT_THAT(value, StructValueIs(_));
  StructValue struct_value = Cast<StructValue>(value);

  // Has returns a status directly instead of a CEL error as in Get.
  EXPECT_THAT(
      struct_value.HasFieldByNumber(999),
      StatusIs(absl::StatusCode::kNotFound, HasSubstr("no_such_field")));
  EXPECT_THAT(
      struct_value.HasFieldByNumber(0x1ffffffff),
      StatusIs(absl::StatusCode::kNotFound, HasSubstr("no_such_field")));
}

TEST_P(ProtoValueWrapTest, ProtoMessageDebugString) {
  ASSERT_OK_AND_ASSIGN(
      auto value, ProtoMessageToValue(value_manager(),
                                      ParseTextOrDie<TestAllTypes>(
                                          R"pb(single_int32: 1, single_int64: 2
                                          )pb")));
  EXPECT_THAT(value, StructValueIs(_));
  StructValue struct_value = Cast<StructValue>(value);

  EXPECT_THAT(struct_value.DebugString(),
              AllOf(HasSubstr("single_int32:"), HasSubstr("1"),
                    HasSubstr("single_int64:"), HasSubstr("2")));
}

TEST_P(ProtoValueWrapTest, ProtoMessageEqual) {
  ASSERT_OK_AND_ASSIGN(
      auto value, ProtoMessageToValue(value_manager(),
                                      ParseTextOrDie<TestAllTypes>(
                                          R"pb(single_int32: 1, single_int64: 2
                                          )pb")));
  ASSERT_OK_AND_ASSIGN(
      auto value2, ProtoMessageToValue(value_manager(),
                                       ParseTextOrDie<TestAllTypes>(
                                           R"pb(single_int32: 1, single_int64: 2
                                           )pb")));
  EXPECT_THAT(value.Equal(value_manager(), value),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(value2.Equal(value_manager(), value),
              IsOkAndHolds(BoolValueIs(true)));
}

TEST_P(ProtoValueWrapTest, ProtoMessageEqualFalse) {
  ASSERT_OK_AND_ASSIGN(
      auto value, ProtoMessageToValue(value_manager(),
                                      ParseTextOrDie<TestAllTypes>(
                                          R"pb(single_int32: 1, single_int64: 2
                                          )pb")));
  ASSERT_OK_AND_ASSIGN(
      auto value2, ProtoMessageToValue(value_manager(),
                                       ParseTextOrDie<TestAllTypes>(
                                           R"pb(single_int32: 2, single_int64: 1
                                           )pb")));
  EXPECT_THAT(value2.Equal(value_manager(), value),
              IsOkAndHolds(BoolValueIs(false)));
}

TEST_P(ProtoValueWrapTest, ProtoMessageForEachField) {
  ASSERT_OK_AND_ASSIGN(
      auto value, ProtoMessageToValue(value_manager(),
                                      ParseTextOrDie<TestAllTypes>(
                                          R"pb(single_int32: 1, single_int64: 2
                                          )pb")));
  EXPECT_THAT(value, StructValueIs(_));
  StructValue struct_value = Cast<StructValue>(value);

  std::vector<std::string> fields;
  auto cb = [&fields](absl::string_view field,
                      ValueView) -> absl::StatusOr<bool> {
    fields.push_back(std::string(field));
    return true;
  };
  ASSERT_OK(struct_value.ForEachField(value_manager(), cb));
  EXPECT_THAT(fields, UnorderedElementsAre("single_int32", "single_int64"));
}

TEST_P(ProtoValueWrapTest, ProtoMessageQualify) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoMessageToValue(value_manager(), ParseTextOrDie<TestAllTypes>(
                                               R"pb(
                                                 standalone_message { bb: 42 }
                                               )pb")));
  EXPECT_THAT(value, StructValueIs(_));
  StructValue struct_value = Cast<StructValue>(value);

  std::vector<SelectQualifier> qualifiers{
      FieldSpecifier{TestAllTypes::kStandaloneMessageFieldNumber,
                     "standalone_message"},
      FieldSpecifier{TestAllTypes::NestedMessage::kBbFieldNumber, "bb"}};

  Value scratch;
  ASSERT_OK_AND_ASSIGN(auto qualify_value,
                       struct_value.Qualify(value_manager(), qualifiers,
                                            /*presence_test=*/false, scratch));

  EXPECT_THAT(Value(qualify_value.first), IntValueIs(42));
}

TEST_P(ProtoValueWrapTest, ProtoMessageQualifyHas) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoMessageToValue(value_manager(), ParseTextOrDie<TestAllTypes>(
                                               R"pb(
                                                 standalone_message { bb: 42 }
                                               )pb")));
  EXPECT_THAT(value, StructValueIs(_));
  StructValue struct_value = Cast<StructValue>(value);

  std::vector<SelectQualifier> qualifiers{
      FieldSpecifier{TestAllTypes::kStandaloneMessageFieldNumber,
                     "standalone_message"},
      FieldSpecifier{TestAllTypes::NestedMessage::kBbFieldNumber, "bb"}};

  Value scratch;
  ASSERT_OK_AND_ASSIGN(auto qualify_value,
                       struct_value.Qualify(value_manager(), qualifiers,
                                            /*presence_test=*/true, scratch));

  EXPECT_THAT(Value(qualify_value.first), BoolValueIs(true));
}

TEST_P(ProtoValueWrapTest, ProtoInt64MapListKeys) {
  if (memory_management() == MemoryManagement::kReferenceCounting) {
    GTEST_SKIP() << "TODO(uncreated-issue/66): use after free";
  }
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoMessageToValue(value_manager(),
                          ParseTextOrDie<TestAllTypes>(
                              R"pb(
                                map_int64_int64 { key: 10 value: 20 })pb")));
  ASSERT_OK_AND_ASSIGN(auto map_value, Cast<StructValue>(value).GetFieldByName(
                                           value_manager(), "map_int64_int64"));

  ASSERT_THAT(map_value, MapValueIs(_));

  ASSERT_OK_AND_ASSIGN(ListValue key_set,
                       Cast<MapValue>(map_value).ListKeys(value_manager()));

  EXPECT_THAT(key_set.Size(), IsOkAndHolds(1));

  ASSERT_OK_AND_ASSIGN(Value key0, key_set.Get(value_manager(), 0));

  EXPECT_THAT(key0, IntValueIs(10));
}

TEST_P(ProtoValueWrapTest, ProtoInt32MapListKeys) {
  if (memory_management() == MemoryManagement::kReferenceCounting) {
    GTEST_SKIP() << "TODO(uncreated-issue/66): use after free";
  }
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoMessageToValue(value_manager(),
                          ParseTextOrDie<TestAllTypes>(
                              R"pb(
                                map_int32_int64 { key: 10 value: 20 })pb")));
  ASSERT_OK_AND_ASSIGN(auto map_value, Cast<StructValue>(value).GetFieldByName(
                                           value_manager(), "map_int32_int64"));

  ASSERT_THAT(map_value, MapValueIs(_));

  ASSERT_OK_AND_ASSIGN(ListValue key_set,
                       Cast<MapValue>(map_value).ListKeys(value_manager()));

  EXPECT_THAT(key_set.Size(), IsOkAndHolds(1));

  ASSERT_OK_AND_ASSIGN(Value key0, key_set.Get(value_manager(), 0));

  EXPECT_THAT(key0, IntValueIs(10));
}

TEST_P(ProtoValueWrapTest, ProtoBoolMapListKeys) {
  if (memory_management() == MemoryManagement::kReferenceCounting) {
    GTEST_SKIP() << "TODO(uncreated-issue/66): use after free";
  }
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoMessageToValue(value_manager(),
                          ParseTextOrDie<TestAllTypes>(
                              R"pb(
                                map_bool_int64 { key: false value: 20 })pb")));
  ASSERT_OK_AND_ASSIGN(auto map_value, Cast<StructValue>(value).GetFieldByName(
                                           value_manager(), "map_bool_int64"));

  ASSERT_THAT(map_value, MapValueIs(_));

  ASSERT_OK_AND_ASSIGN(ListValue key_set,
                       Cast<MapValue>(map_value).ListKeys(value_manager()));

  EXPECT_THAT(key_set.Size(), IsOkAndHolds(1));

  ASSERT_OK_AND_ASSIGN(Value key0, key_set.Get(value_manager(), 0));

  EXPECT_THAT(key0, BoolValueIs(false));
}

TEST_P(ProtoValueWrapTest, ProtoUint32MapListKeys) {
  if (memory_management() == MemoryManagement::kReferenceCounting) {
    GTEST_SKIP() << "TODO(uncreated-issue/66): use after free";
  }
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoMessageToValue(value_manager(),
                          ParseTextOrDie<TestAllTypes>(
                              R"pb(
                                map_uint32_int64 { key: 11 value: 20 })pb")));
  ASSERT_OK_AND_ASSIGN(auto map_value,
                       Cast<StructValue>(value).GetFieldByName(
                           value_manager(), "map_uint32_int64"));

  ASSERT_THAT(map_value, MapValueIs(_));

  ASSERT_OK_AND_ASSIGN(ListValue key_set,
                       Cast<MapValue>(map_value).ListKeys(value_manager()));

  EXPECT_THAT(key_set.Size(), IsOkAndHolds(1));

  ASSERT_OK_AND_ASSIGN(Value key0, key_set.Get(value_manager(), 0));

  EXPECT_THAT(key0, UintValueIs(11));
}

TEST_P(ProtoValueWrapTest, ProtoUint64MapListKeys) {
  if (memory_management() == MemoryManagement::kReferenceCounting) {
    GTEST_SKIP() << "TODO(uncreated-issue/66): use after free";
  }
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoMessageToValue(value_manager(),
                          ParseTextOrDie<TestAllTypes>(
                              R"pb(
                                map_uint64_int64 { key: 11 value: 20 })pb")));
  ASSERT_OK_AND_ASSIGN(auto map_value,
                       Cast<StructValue>(value).GetFieldByName(
                           value_manager(), "map_uint64_int64"));

  ASSERT_THAT(map_value, MapValueIs(_));

  ASSERT_OK_AND_ASSIGN(ListValue key_set,
                       Cast<MapValue>(map_value).ListKeys(value_manager()));

  EXPECT_THAT(key_set.Size(), IsOkAndHolds(1));

  ASSERT_OK_AND_ASSIGN(Value key0, key_set.Get(value_manager(), 0));

  EXPECT_THAT(key0, UintValueIs(11));
}

TEST_P(ProtoValueWrapTest, ProtoStringMapListKeys) {
  if (memory_management() == MemoryManagement::kReferenceCounting) {
    GTEST_SKIP() << "TODO(uncreated-issue/66): use after free";
  }
  ASSERT_OK_AND_ASSIGN(
      auto value, ProtoMessageToValue(
                      value_manager(),
                      ParseTextOrDie<TestAllTypes>(
                          R"pb(
                            map_string_int64 { key: "key1" value: 20 })pb")));
  ASSERT_OK_AND_ASSIGN(auto map_value,
                       Cast<StructValue>(value).GetFieldByName(
                           value_manager(), "map_string_int64"));

  ASSERT_THAT(map_value, MapValueIs(_));

  ASSERT_OK_AND_ASSIGN(ListValue key_set,
                       Cast<MapValue>(map_value).ListKeys(value_manager()));

  EXPECT_THAT(key_set.Size(), IsOkAndHolds(1));

  ASSERT_OK_AND_ASSIGN(Value key0, key_set.Get(value_manager(), 0));

  EXPECT_THAT(key0, StringValueIs("key1"));
}

TEST_P(ProtoValueWrapTest, ProtoMapDebugString) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoMessageToValue(value_manager(),
                          ParseTextOrDie<TestAllTypes>(
                              R"pb(
                                map_int64_int64 { key: 10 value: 20 }
                                map_int64_int64 { key: 12 value: 24 }
                              )pb")));
  ASSERT_OK_AND_ASSIGN(auto field_value,
                       Cast<StructValue>(value).GetFieldByName(
                           value_manager(), "map_int64_int64"));

  ASSERT_THAT(field_value, MapValueIs(_));

  MapValue map_value = Cast<MapValue>(field_value);

  EXPECT_THAT(map_value.DebugString(),
              AllOf(HasSubstr("10:"), HasSubstr("20"), HasSubstr("12:"),
                    HasSubstr("24")));
}

TEST_P(ProtoValueWrapTest, ProtoMapIterator) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoMessageToValue(value_manager(),
                          ParseTextOrDie<TestAllTypes>(
                              R"pb(
                                map_int64_int64 { key: 10 value: 20 }
                                map_int64_int64 { key: 12 value: 24 }
                              )pb")));
  ASSERT_OK_AND_ASSIGN(auto field_value,
                       Cast<StructValue>(value).GetFieldByName(
                           value_manager(), "map_int64_int64"));

  ASSERT_THAT(field_value, MapValueIs(_));

  MapValue map_value = Cast<MapValue>(field_value);

  std::vector<Value> keys;

  ASSERT_OK_AND_ASSIGN(auto iter, map_value.NewIterator(value_manager()));

  while (iter->HasNext()) {
    ASSERT_OK_AND_ASSIGN(keys.emplace_back(), iter->Next(value_manager()));
  }

  EXPECT_THAT(keys, UnorderedElementsAre(IntValueIs(10), IntValueIs(12)));
}

TEST_P(ProtoValueWrapTest, ProtoMapForEach) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoMessageToValue(value_manager(),
                          ParseTextOrDie<TestAllTypes>(
                              R"pb(
                                map_int64_int64 { key: 10 value: 20 }
                                map_int64_int64 { key: 12 value: 24 }
                              )pb")));
  ASSERT_OK_AND_ASSIGN(auto field_value,
                       Cast<StructValue>(value).GetFieldByName(
                           value_manager(), "map_int64_int64"));

  ASSERT_THAT(field_value, MapValueIs(_));

  MapValue map_value = Cast<MapValue>(field_value);

  std::vector<std::pair<Value, Value>> pairs;

  auto cb = [&pairs](ValueView key, ValueView value) -> absl::StatusOr<bool> {
    pairs.push_back(std::pair<Value, Value>(key, value));
    return true;
  };
  ASSERT_OK(map_value.ForEach(value_manager(), cb));

  EXPECT_THAT(pairs,
              UnorderedElementsAre(Pair(IntValueIs(10), IntValueIs(20)),
                                   Pair(IntValueIs(12), IntValueIs(24))));
}

TEST_P(ProtoValueWrapTest, ProtoListIterator) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoMessageToValue(value_manager(), ParseTextOrDie<TestAllTypes>(
                                               R"pb(
                                                 repeated_int64: 1
                                                 repeated_int64: 2
                                               )pb")));
  ASSERT_OK_AND_ASSIGN(auto field_value,
                       Cast<StructValue>(value).GetFieldByName(
                           value_manager(), "repeated_int64"));

  ASSERT_THAT(field_value, ListValueIs(_));

  ListValue list_value = Cast<ListValue>(field_value);

  std::vector<Value> elements;

  ASSERT_OK_AND_ASSIGN(auto iter, list_value.NewIterator(value_manager()));

  while (iter->HasNext()) {
    ASSERT_OK_AND_ASSIGN(elements.emplace_back(), iter->Next(value_manager()));
  }

  EXPECT_THAT(elements, ElementsAre(IntValueIs(1), IntValueIs(2)));
}

TEST_P(ProtoValueWrapTest, ProtoListForEachWithIndex) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoMessageToValue(value_manager(), ParseTextOrDie<TestAllTypes>(
                                               R"pb(
                                                 repeated_int64: 1
                                                 repeated_int64: 2
                                               )pb")));
  ASSERT_OK_AND_ASSIGN(auto field_value,
                       Cast<StructValue>(value).GetFieldByName(
                           value_manager(), "repeated_int64"));

  ASSERT_THAT(field_value, ListValueIs(_));

  ListValue list_value = Cast<ListValue>(field_value);

  std::vector<std::pair<size_t, Value>> elements;

  auto cb = [&elements](size_t index, ValueView value) -> absl::StatusOr<bool> {
    elements.push_back(std::pair<size_t, Value>(index, value));
    return true;
  };

  ASSERT_OK(list_value.ForEach(value_manager(), cb));

  EXPECT_THAT(elements,
              ElementsAre(Pair(0, IntValueIs(1)), Pair(1, IntValueIs(2))));
}

TEST_P(ProtoValueWrapTest, ProtoListDebugString) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoMessageToValue(value_manager(), ParseTextOrDie<TestAllTypes>(
                                               R"pb(
                                                 repeated_int64: 1
                                                 repeated_int64: 2
                                               )pb")));
  ASSERT_OK_AND_ASSIGN(auto field_value,
                       Cast<StructValue>(value).GetFieldByName(
                           value_manager(), "repeated_int64"));

  ASSERT_THAT(field_value, ListValueIs(_));

  ListValue list_value = Cast<ListValue>(field_value);

  EXPECT_THAT(list_value.DebugString(), AllOf(HasSubstr("1"), HasSubstr("2")));
}

INSTANTIATE_TEST_SUITE_P(ProtoValueTest, ProtoValueWrapTest,
                         testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting),
                         ProtoValueTest::ToString);

struct DefaultArenaDeleter {
  template <typename T>
  void operator()(T* message) const {
    if (arena == nullptr) {
      delete message;
    }
  }

  google::protobuf::Arena* arena = nullptr;
};

template <typename T>
using ArenaUniquePtr = std::unique_ptr<T, DefaultArenaDeleter>;

template <typename T>
ArenaUniquePtr<T> WrapArenaUnique(T* message) {
  return ArenaUniquePtr<T>(message, DefaultArenaDeleter{message->GetArena()});
}

template <typename T>
absl::StatusOr<ArenaUniquePtr<T>> WrapArenaUnique(absl::StatusOr<T*> message) {
  if (!message.ok()) {
    return message.status();
  }
  return WrapArenaUnique(*message);
}

class ProtoValueUnwrapTest : public ProtoValueTest {};

TEST_P(ProtoValueUnwrapTest, ProtoBoolValueFromValue) {
  google::protobuf::BoolValue message;
  EXPECT_THAT(ProtoMessageFromValue(BoolValueView{true}, message), IsOk());
  EXPECT_EQ(message.value(), true);

  EXPECT_THAT(
      WrapArenaUnique(ProtoMessageFromValue(
          BoolValueView{true}, ProtoMemoryManagerArena(memory_manager()))),
      IsOkAndHolds(Pointee(EqualsProto(message))));

  EXPECT_THAT(ProtoMessageFromValue(UnknownValueView{}, message),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ProtoValueUnwrapTest, ProtoInt32ValueFromValue) {
  google::protobuf::Int32Value message;
  EXPECT_THAT(ProtoMessageFromValue(IntValueView{1}, message), IsOk());
  EXPECT_EQ(message.value(), 1);
  EXPECT_THAT(
      ProtoMessageFromValue(
          IntValueView{
              static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1},
          message),
      StatusIs(absl::StatusCode::kOutOfRange));

  EXPECT_THAT(ProtoMessageFromValue(UnknownValueView{}, message),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ProtoValueUnwrapTest, ProtoInt64ValueFromValue) {
  google::protobuf::Int64Value message;
  EXPECT_THAT(ProtoMessageFromValue(IntValueView{1}, message), IsOk());
  EXPECT_EQ(message.value(), true);

  EXPECT_THAT(WrapArenaUnique(ProtoMessageFromValue(
                  IntValueView{1}, ProtoMemoryManagerArena(memory_manager()))),
              IsOkAndHolds(Pointee(EqualsProto(message))));

  EXPECT_THAT(ProtoMessageFromValue(UnknownValueView{}, message),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ProtoValueUnwrapTest, ProtoUInt32ValueFromValue) {
  google::protobuf::UInt32Value message;
  EXPECT_THAT(ProtoMessageFromValue(UintValueView{1}, message), IsOk());
  EXPECT_EQ(message.value(), 1);
  EXPECT_THAT(
      ProtoMessageFromValue(
          UintValueView{
              static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1},
          message),
      StatusIs(absl::StatusCode::kOutOfRange));

  EXPECT_THAT(ProtoMessageFromValue(UnknownValueView{}, message),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ProtoValueUnwrapTest, ProtoUInt64ValueFromValue) {
  google::protobuf::UInt64Value message;
  EXPECT_THAT(ProtoMessageFromValue(UintValueView{1}, message), IsOk());
  EXPECT_EQ(message.value(), 1);

  EXPECT_THAT(WrapArenaUnique(ProtoMessageFromValue(
                  UintValueView{1}, ProtoMemoryManagerArena(memory_manager()))),
              IsOkAndHolds(Pointee(EqualsProto(message))));

  EXPECT_THAT(ProtoMessageFromValue(UnknownValueView{}, message),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ProtoValueUnwrapTest, ProtoFloatValueFromValue) {
  google::protobuf::FloatValue message;
  EXPECT_THAT(ProtoMessageFromValue(DoubleValueView{1}, message), IsOk());
  EXPECT_EQ(message.value(), 1);

  EXPECT_THAT(ProtoMessageFromValue(UnknownValueView{}, message),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ProtoValueUnwrapTest, ProtoDoubleValueFromValue) {
  google::protobuf::DoubleValue message;
  EXPECT_THAT(ProtoMessageFromValue(DoubleValueView{1}, message), IsOk());
  EXPECT_EQ(message.value(), 1);

  EXPECT_THAT(
      WrapArenaUnique(ProtoMessageFromValue(
          DoubleValueView{1}, ProtoMemoryManagerArena(memory_manager()))),
      IsOkAndHolds(Pointee(EqualsProto(message))));

  EXPECT_THAT(ProtoMessageFromValue(UnknownValueView{}, message),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ProtoValueUnwrapTest, ProtoBytesValueFromValue) {
  google::protobuf::BytesValue message;
  EXPECT_THAT(ProtoMessageFromValue(BytesValueView{"foo"}, message), IsOk());
  EXPECT_EQ(message.value(), "foo");

  EXPECT_THAT(
      WrapArenaUnique(ProtoMessageFromValue(
          BytesValueView{"foo"}, ProtoMemoryManagerArena(memory_manager()))),
      IsOkAndHolds(Pointee(EqualsProto(message))));

  EXPECT_THAT(ProtoMessageFromValue(UnknownValueView{}, message),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ProtoValueUnwrapTest, ProtoStringValueFromValue) {
  google::protobuf::StringValue message;
  EXPECT_THAT(ProtoMessageFromValue(StringValueView{"foo"}, message), IsOk());
  EXPECT_EQ(message.value(), "foo");

  EXPECT_THAT(
      WrapArenaUnique(ProtoMessageFromValue(
          StringValueView{"foo"}, ProtoMemoryManagerArena(memory_manager()))),
      IsOkAndHolds(Pointee(EqualsProto(message))));

  EXPECT_THAT(ProtoMessageFromValue(UnknownValueView{}, message),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ProtoValueUnwrapTest, ProtoDurationFromValue) {
  google::protobuf::Duration message;
  EXPECT_THAT(
      ProtoMessageFromValue(
          DurationValueView{absl::Seconds(1) + absl::Nanoseconds(1)}, message),
      IsOk());
  EXPECT_EQ(message.seconds(), 1);
  EXPECT_EQ(message.nanos(), 1);

  EXPECT_THAT(WrapArenaUnique(ProtoMessageFromValue(
                  DurationValueView{absl::Seconds(1) + absl::Nanoseconds(1)},
                  ProtoMemoryManagerArena(memory_manager()))),
              IsOkAndHolds(Pointee(EqualsProto(message))));

  EXPECT_THAT(ProtoMessageFromValue(UnknownValueView{}, message),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ProtoValueUnwrapTest, ProtoTimestampFromValue) {
  google::protobuf::Timestamp message;
  EXPECT_THAT(ProtoMessageFromValue(
                  TimestampValueView{absl::UnixEpoch() + absl::Seconds(1) +
                                     absl::Nanoseconds(1)},
                  message),
              IsOk());
  EXPECT_EQ(message.seconds(), 1);
  EXPECT_EQ(message.nanos(), 1);

  EXPECT_THAT(WrapArenaUnique(ProtoMessageFromValue(
                  TimestampValueView{absl::UnixEpoch() + absl::Seconds(1) +
                                     absl::Nanoseconds(1)},
                  ProtoMemoryManagerArena(memory_manager()))),
              IsOkAndHolds(Pointee(EqualsProto(message))));

  EXPECT_THAT(ProtoMessageFromValue(UnknownValueView{}, message),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ProtoValueUnwrapTest, ProtoValueFromValue) {
  google::protobuf::Value message;
  EXPECT_THAT(ProtoMessageFromValue(NullValueView{}, message), IsOk());
  EXPECT_TRUE(message.has_null_value());
  EXPECT_THAT(ProtoMessageFromValue(BoolValueView{true}, message), IsOk());
  EXPECT_EQ(message.bool_value(), true);
  EXPECT_THAT(ProtoMessageFromValue(DoubleValueView{1}, message), IsOk());
  EXPECT_EQ(message.number_value(), 1);
  EXPECT_THAT(ProtoMessageFromValue(ListValueView{}, message), IsOk());
  EXPECT_TRUE(message.has_list_value());
  EXPECT_TRUE(message.list_value().values().empty());
  EXPECT_THAT(ProtoMessageFromValue(MapValueView{}, message), IsOk());
  EXPECT_TRUE(message.has_struct_value());
  EXPECT_TRUE(message.struct_value().fields().empty());

  message.set_null_value(google::protobuf::NULL_VALUE);
  EXPECT_THAT(WrapArenaUnique(ProtoMessageFromValue(
                  NullValueView{}, ProtoMemoryManagerArena(memory_manager()))),
              IsOkAndHolds(Pointee(EqualsProto(message))));

  EXPECT_THAT(ProtoMessageFromValue(UnknownValueView{}, message),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(ProtoValueUnwrapTest, ProtoListValueFromValue) {
  google::protobuf::ListValue message;
  EXPECT_THAT(ProtoMessageFromValue(ListValueView{}, message), IsOk());
  EXPECT_TRUE(message.values().empty());

  EXPECT_THAT(WrapArenaUnique(ProtoMessageFromValue(
                  ListValueView{}, ProtoMemoryManagerArena(memory_manager()))),
              IsOkAndHolds(Pointee(EqualsProto(message))));

  EXPECT_THAT(ProtoMessageFromValue(UnknownValueView{}, message),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(ProtoValueUnwrapTest, ProtoStructFromValue) {
  google::protobuf::Struct message;
  EXPECT_THAT(ProtoMessageFromValue(MapValueView{}, message), IsOk());
  EXPECT_TRUE(message.fields().empty());

  EXPECT_THAT(WrapArenaUnique(ProtoMessageFromValue(
                  MapValueView{}, ProtoMemoryManagerArena(memory_manager()))),
              IsOkAndHolds(Pointee(EqualsProto(message))));

  EXPECT_THAT(ProtoMessageFromValue(UnknownValueView{}, message),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(ProtoValueUnwrapTest, ProtoAnyFromValue) {
  google::protobuf::Any message;
  EXPECT_THAT(ProtoMessageFromValue(BoolValueView{false}, message), IsOk());
  EXPECT_EQ(message.type_url(),
            "type.googleapis.com/google.protobuf.BoolValue");
  EXPECT_TRUE(message.value().empty());

  EXPECT_THAT(ProtoMessageFromValue(UnknownValueView{}, message),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(ProtoValueUnwrapTest, ProtoMessageFromValue) {
  TestAllTypes want;
  want.set_single_bool(true);
  want.set_single_int64(1);
  ASSERT_OK_AND_ASSIGN(auto value, ProtoMessageToValue(value_manager(), want));
  TestAllTypes got;
  EXPECT_THAT(ProtoMessageFromValue(value, got), IsOk());
  EXPECT_THAT(got, EqualsProto(want));

  EXPECT_THAT(ProtoMessageFromValue(UnknownValueView{}, got),
              StatusIs(absl::StatusCode::kInvalidArgument));

  EXPECT_THAT(WrapArenaUnique(ProtoMessageFromValue(
                  value, ProtoMemoryManagerArena(memory_manager()))),
              IsOkAndHolds(Pointee(EqualsProto(want))));
}

TEST_P(ProtoValueUnwrapTest, ProtoMessageFromLegacyValue) {
  TestAllTypes want;
  want.set_single_bool(true);
  want.set_single_int64(1);
  auto value = Value(common_internal::LegacyStructValue{
      reinterpret_cast<uintptr_t>(&want) |
          base_internal::kMessageWrapperTagMessageValue,
      reinterpret_cast<uintptr_t>(
          &google::api::expr::runtime::GetGenericProtoTypeInfoInstance())});
  TestAllTypes got;
  EXPECT_THAT(ProtoMessageFromValue(value, got), IsOk());
  EXPECT_THAT(got, EqualsProto(want));
}

TEST_P(ProtoValueUnwrapTest, ProtoMessageLiteFromLegacyValue) {
  TestAllTypes want;
  want.set_single_bool(true);
  want.set_single_int64(1);
  auto value = Value(common_internal::LegacyStructValue{
      reinterpret_cast<uintptr_t>(&want),
      reinterpret_cast<uintptr_t>(
          &google::api::expr::runtime::GetGenericProtoTypeInfoInstance())});
  TestAllTypes got;
  EXPECT_THAT(ProtoMessageFromValue(value, got), IsOk());
  EXPECT_THAT(got, EqualsProto(want));
}

TEST_P(ProtoValueUnwrapTest, ProtoEnumFromValue) {
  EXPECT_THAT(ProtoEnumFromValue<google::protobuf::NullValue>(NullValueView{}),
              IsOkAndHolds(Eq(google::protobuf::NULL_VALUE)));
  EXPECT_THAT(
      ProtoEnumFromValue<google::protobuf::NullValue>(IntValueView{0xdeadbeef}),
      IsOkAndHolds(Eq(google::protobuf::NULL_VALUE)));
  EXPECT_THAT(
      ProtoEnumFromValue<google::protobuf::NullValue>(StringValueView{}),
      StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(ProtoEnumFromValue<TestAllTypes::NestedEnum>(IntValueView{1}),
              IsOkAndHolds(Eq(TestAllTypes::BAR)));
  EXPECT_THAT(ProtoEnumFromValue<TestAllTypes::NestedEnum>(IntValueView{1000}),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(ProtoEnumFromValue<TestAllTypes::NestedEnum>(StringValueView{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

INSTANTIATE_TEST_SUITE_P(ProtoValueTest, ProtoValueUnwrapTest,
                         testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting),
                         ProtoValueTest::ToString);

}  // namespace
}  // namespace cel::extensions
