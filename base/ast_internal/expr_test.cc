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

#include "base/ast_internal/expr.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/types/variant.h"
#include "common/ast.h"
#include "internal/testing.h"

namespace cel {
namespace ast_internal {
namespace {

TEST(AstTest, ExprConstructionConstant) {
  Constant constant;
  constant.set_bool_value(true);
  Expr expr(1, std::move(constant));
  ASSERT_TRUE(absl::holds_alternative<Constant>(expr.expr_kind()));
  ASSERT_TRUE(absl::get<Constant>(expr.expr_kind()).has_bool_value());
  ASSERT_TRUE(absl::get<Constant>(expr.expr_kind()).bool_value());
}

TEST(AstTest, ExprConstructionIdent) {
  Expr expr(1, Ident("var"));
  ASSERT_TRUE(absl::holds_alternative<Ident>(expr.expr_kind()));
  ASSERT_EQ(absl::get<Ident>(expr.expr_kind()).name(), "var");
}

TEST(AstTest, ExprConstructionSelect) {
  Expr expr(1, Select(std::make_unique<Expr>(2, Ident("var")), "field"));
  ASSERT_TRUE(absl::holds_alternative<Select>(expr.expr_kind()));
  const auto& select = absl::get<Select>(expr.expr_kind());
  ASSERT_TRUE(absl::holds_alternative<Ident>(select.operand().expr_kind()));
  ASSERT_EQ(absl::get<Ident>(select.operand().expr_kind()).name(), "var");
  ASSERT_EQ(select.field(), "field");
}

TEST(AstTest, SelectMutableOperand) {
  Select select;
  select.mutable_operand().set_expr_kind(Ident("var"));
  ASSERT_TRUE(absl::holds_alternative<Ident>(select.operand().expr_kind()));
  ASSERT_EQ(absl::get<Ident>(select.operand().expr_kind()).name(), "var");
}

TEST(AstTest, SelectDefaultOperand) {
  Select select;
  EXPECT_EQ(select.operand(), Expr());
}

TEST(AstTest, SelectComparatorTestOnly) {
  Select select;
  select.set_test_only(true);
  EXPECT_FALSE(select == Select());
}

TEST(AstTest, SelectComparatorField) {
  Select select;
  select.set_field("field");
  EXPECT_FALSE(select == Select());
}

TEST(AstTest, ExprConstructionCall) {
  Expr expr(1, Call(std::make_unique<Expr>(2, Ident("var")), "function", {}));
  ASSERT_TRUE(absl::holds_alternative<Call>(expr.expr_kind()));
  const auto& call = absl::get<Call>(expr.expr_kind());
  ASSERT_TRUE(absl::holds_alternative<Ident>(call.target().expr_kind()));
  ASSERT_EQ(absl::get<Ident>(call.target().expr_kind()).name(), "var");
  ASSERT_EQ(call.function(), "function");
  ASSERT_TRUE(call.args().empty());
}

TEST(AstTest, CallMutableTarget) {
  Call call;
  call.mutable_target().set_expr_kind(Ident("var"));
  ASSERT_TRUE(absl::holds_alternative<Ident>(call.target().expr_kind()));
  ASSERT_EQ(absl::get<Ident>(call.target().expr_kind()).name(), "var");
}

TEST(AstTest, CallDefaultTarget) { EXPECT_EQ(Call().target(), Expr()); }

TEST(AstTest, CallComparatorTarget) {
  Call call;
  call.set_function("function");
  EXPECT_FALSE(call == Call());
}

TEST(AstTest, CallComparatorArgs) {
  Call call;
  call.mutable_args().emplace_back(Expr());
  EXPECT_FALSE(call == Call());
}

TEST(AstTest, CallComparatorFunction) {
  Call call;
  call.set_function("function");
  EXPECT_FALSE(call == Call());
}

TEST(AstTest, ExprConstructionCreateList) {
  CreateList create_list;
  create_list.mutable_elements().emplace_back(Expr(2, Ident("var1")));
  create_list.mutable_elements().emplace_back(Expr(3, Ident("var2")));
  create_list.mutable_elements().emplace_back(Expr(4, Ident("var3")));
  Expr expr(1, std::move(create_list));
  ASSERT_TRUE(absl::holds_alternative<CreateList>(expr.expr_kind()));
  const auto& elements = absl::get<CreateList>(expr.expr_kind()).elements();
  ASSERT_EQ(absl::get<Ident>(elements[0].expr_kind()).name(), "var1");
  ASSERT_EQ(absl::get<Ident>(elements[1].expr_kind()).name(), "var2");
  ASSERT_EQ(absl::get<Ident>(elements[2].expr_kind()).name(), "var3");
}

TEST(AstTest, ExprConstructionCreateStruct) {
  CreateStruct create_struct;
  create_struct.set_message_name("name");
  create_struct.mutable_entries().emplace_back(CreateStruct::Entry(
      1, "key1", std::make_unique<Expr>(2, Ident("value1"))));
  create_struct.mutable_entries().emplace_back(CreateStruct::Entry(
      3, "key2", std::make_unique<Expr>(4, Ident("value2"))));
  create_struct.mutable_entries().emplace_back(
      CreateStruct::Entry(5, std::make_unique<Expr>(6, Ident("key3")),
                          std::make_unique<Expr>(6, Ident("value3"))));
  Expr expr(1, std::move(create_struct));
  ASSERT_TRUE(absl::holds_alternative<CreateStruct>(expr.expr_kind()));
  const auto& entries = absl::get<CreateStruct>(expr.expr_kind()).entries();
  ASSERT_EQ(absl::get<std::string>(entries[0].key_kind()), "key1");
  ASSERT_EQ(absl::get<Ident>(entries[0].value().expr_kind()).name(), "value1");
  ASSERT_EQ(absl::get<std::string>(entries[1].key_kind()), "key2");
  ASSERT_EQ(absl::get<Ident>(entries[1].value().expr_kind()).name(), "value2");
  ASSERT_EQ(
      absl::get<Ident>(
          absl::get<std::unique_ptr<Expr>>(entries[2].key_kind())->expr_kind())
          .name(),
      "key3");
  ASSERT_EQ(absl::get<Ident>(entries[2].value().expr_kind()).name(), "value3");
}

TEST(AstTest, ExprCreateStructEntryDefaults) {
  CreateStruct::Entry entry;
  EXPECT_TRUE(entry.field_key().empty());
  EXPECT_EQ(entry.map_key(), Expr());
  EXPECT_EQ(entry.value(), Expr());
}

TEST(AstTest, CreateStructEntryMutableValue) {
  CreateStruct::Entry entry;
  entry.mutable_value().set_expr_kind(Ident("var"));
  ASSERT_TRUE(absl::holds_alternative<Ident>(entry.value().expr_kind()));
  ASSERT_EQ(absl::get<Ident>(entry.value().expr_kind()).name(), "var");
}

TEST(AstTest, CreateStructEntryMutableMapKey) {
  CreateStruct::Entry entry;
  entry.mutable_map_key().set_expr_kind(Ident("key"));
  ASSERT_TRUE(absl::holds_alternative<Ident>(entry.map_key().expr_kind()));
  ASSERT_EQ(absl::get<Ident>(entry.map_key().expr_kind()).name(), "key");
  entry.mutable_map_key().set_expr_kind(Ident("new_key"));
  ASSERT_TRUE(absl::holds_alternative<Ident>(entry.map_key().expr_kind()));
  ASSERT_EQ(absl::get<Ident>(entry.map_key().expr_kind()).name(), "new_key");
}

TEST(AstTest, CreateStructEntryFieldKeyGetterSetterTest) {
  CreateStruct::Entry entry;
  entry.set_field_key("key");
  EXPECT_EQ(entry.field_key(), "key");
}

TEST(AstTest, CreateStructEntryComparatorMapKeySuccess) {
  CreateStruct::Entry entry1;
  entry1.mutable_map_key().set_expr_kind(Ident("key"));
  CreateStruct::Entry entry2;
  entry2.mutable_map_key().set_expr_kind(Ident("key"));
  EXPECT_EQ(entry1, entry2);
}

TEST(AstTest, CreateStructEntryComparatorMapKeyFailure) {
  CreateStruct::Entry entry1;
  entry1.mutable_map_key().set_expr_kind(Ident("key"));
  CreateStruct::Entry entry2;
  entry2.mutable_map_key().set_expr_kind(Ident("other_key"));
  EXPECT_NE(entry1, entry2);
}

TEST(AstTest, CreateStructEntryComparatorFieldKeySuccess) {
  CreateStruct::Entry entry1;
  entry1.set_field_key("key");
  CreateStruct::Entry entry2;
  entry2.set_field_key("key");
  EXPECT_EQ(entry1, entry2);
}

TEST(AstTest, CreateStructEntryComparatorFieldKeyFailure) {
  CreateStruct::Entry entry1;
  entry1.set_field_key("key");
  CreateStruct::Entry entry2;
  entry2.set_field_key("other_key");
  EXPECT_NE(entry1, entry2);
}

TEST(AstTest, CreateStructEntryComparatorFieldKeyDiffersFromMapKey) {
  CreateStruct::Entry entry1;
  entry1.set_field_key("");
  CreateStruct::Entry entry2;
  entry2.mutable_map_key();
  EXPECT_NE(entry1, entry2);
}

TEST(AstTest, CreateStructEntryComparatorMapKeyDiffersFromFieldKey) {
  CreateStruct::Entry entry1;
  entry1.mutable_map_key();
  CreateStruct::Entry entry2;
  entry2.set_field_key("");
  EXPECT_NE(entry1, entry2);
}

TEST(AstTest, CreateStructEntryComparatorValueSuccess) {
  CreateStruct::Entry entry1;
  entry1.mutable_value().set_expr_kind(Ident("key"));
  CreateStruct::Entry entry2;
  entry2.mutable_value().set_expr_kind(Ident("key"));
  EXPECT_EQ(entry1, entry2);
}

TEST(AstTest, CreateStructEntryComparatorValueFailure) {
  CreateStruct::Entry entry1;
  entry1.mutable_value().set_expr_kind(Ident("key"));
  CreateStruct::Entry entry2;
  entry2.mutable_value().set_expr_kind(Ident("other_key"));
  EXPECT_NE(entry1, entry2);
}

TEST(AstTest, ExprConstructionComprehension) {
  Comprehension comprehension;
  comprehension.set_iter_var("iter_var");
  comprehension.set_iter_range(std::make_unique<Expr>(1, Ident("range")));
  comprehension.set_accu_var("accu_var");
  comprehension.set_accu_init(std::make_unique<Expr>(2, Ident("init")));
  comprehension.set_loop_condition(std::make_unique<Expr>(3, Ident("cond")));
  comprehension.set_loop_step(std::make_unique<Expr>(4, Ident("step")));
  comprehension.set_result(std::make_unique<Expr>(5, Ident("result")));
  Expr expr(6, std::move(comprehension));
  ASSERT_TRUE(absl::holds_alternative<Comprehension>(expr.expr_kind()));
  auto& created_expr = absl::get<Comprehension>(expr.expr_kind());
  ASSERT_EQ(created_expr.iter_var(), "iter_var");
  ASSERT_EQ(absl::get<Ident>(created_expr.iter_range().expr_kind()).name(),
            "range");
  ASSERT_EQ(created_expr.accu_var(), "accu_var");
  ASSERT_EQ(absl::get<Ident>(created_expr.accu_init().expr_kind()).name(),
            "init");
  ASSERT_EQ(absl::get<Ident>(created_expr.loop_condition().expr_kind()).name(),
            "cond");
  ASSERT_EQ(absl::get<Ident>(created_expr.loop_step().expr_kind()).name(),
            "step");
  ASSERT_EQ(absl::get<Ident>(created_expr.result().expr_kind()).name(),
            "result");
}

TEST(AstTest, ComprehensionMutableConstruction) {
  Comprehension comprehension;
  comprehension.mutable_iter_range().set_expr_kind(Ident("var"));
  ASSERT_TRUE(
      absl::holds_alternative<Ident>(comprehension.iter_range().expr_kind()));
  ASSERT_EQ(absl::get<Ident>(comprehension.iter_range().expr_kind()).name(),
            "var");
  comprehension.mutable_accu_init().set_expr_kind(Ident("var"));
  ASSERT_TRUE(
      absl::holds_alternative<Ident>(comprehension.accu_init().expr_kind()));
  ASSERT_EQ(absl::get<Ident>(comprehension.accu_init().expr_kind()).name(),
            "var");
  comprehension.mutable_loop_condition().set_expr_kind(Ident("var"));
  ASSERT_TRUE(absl::holds_alternative<Ident>(
      comprehension.loop_condition().expr_kind()));
  ASSERT_EQ(absl::get<Ident>(comprehension.loop_condition().expr_kind()).name(),
            "var");
  comprehension.mutable_loop_step().set_expr_kind(Ident("var"));
  ASSERT_TRUE(
      absl::holds_alternative<Ident>(comprehension.loop_step().expr_kind()));
  ASSERT_EQ(absl::get<Ident>(comprehension.loop_step().expr_kind()).name(),
            "var");
  comprehension.mutable_result().set_expr_kind(Ident("var"));
  ASSERT_TRUE(
      absl::holds_alternative<Ident>(comprehension.result().expr_kind()));
  ASSERT_EQ(absl::get<Ident>(comprehension.result().expr_kind()).name(), "var");
}

TEST(AstTest, ComprehensionDefaults) {
  Comprehension comprehension;
  EXPECT_TRUE(comprehension.iter_var().empty());
  EXPECT_EQ(comprehension.iter_range(), Expr());
  EXPECT_TRUE(comprehension.accu_var().empty());
  EXPECT_EQ(comprehension.accu_init(), Expr());
  EXPECT_EQ(comprehension.loop_condition(), Expr());
  EXPECT_EQ(comprehension.loop_step(), Expr());
  EXPECT_EQ(comprehension.result(), Expr());
}

TEST(AstTest, ComprehenesionComparatorIterVar) {
  Comprehension comprehension;
  comprehension.set_iter_var("var");
  EXPECT_FALSE(comprehension == Comprehension());
}

TEST(AstTest, ComprehenesionComparatorAccuVar) {
  Comprehension comprehension;
  comprehension.set_accu_var("var");
  EXPECT_FALSE(comprehension == Comprehension());
}

TEST(AstTest, ExprMoveTest) {
  Expr expr(1, Ident("var"));
  ASSERT_TRUE(absl::holds_alternative<Ident>(expr.expr_kind()));
  ASSERT_EQ(absl::get<Ident>(expr.expr_kind()).name(), "var");
  Expr new_expr = std::move(expr);
  ASSERT_TRUE(absl::holds_alternative<Ident>(new_expr.expr_kind()));
  ASSERT_EQ(absl::get<Ident>(new_expr.expr_kind()).name(), "var");
}

TEST(AstTest, ExprDefaults) {
  Expr expr;
  EXPECT_EQ(expr.const_expr(), Constant());
  EXPECT_EQ(expr.ident_expr(), Ident(""));
  EXPECT_EQ(expr.select_expr(), Select());
  EXPECT_EQ(expr.call_expr(), Call());
  EXPECT_EQ(expr.list_expr(), CreateList());
  EXPECT_EQ(expr.struct_expr(), CreateStruct());
  EXPECT_EQ(expr.comprehension_expr(), Comprehension());
}

TEST(AstTest, ParsedExpr) {
  ParsedExpr parsed_expr;
  parsed_expr.set_expr(Expr(1, Ident("name")));
  auto& source_info = parsed_expr.mutable_source_info();
  source_info.set_syntax_version("syntax_version");
  source_info.set_location("location");
  source_info.set_line_offsets({1, 2, 3});
  source_info.set_positions({{1, 1}, {2, 2}});
  ASSERT_TRUE(absl::holds_alternative<Ident>(parsed_expr.expr().expr_kind()));
  ASSERT_EQ(absl::get<Ident>(parsed_expr.expr().expr_kind()).name(), "name");
  ASSERT_EQ(parsed_expr.source_info().syntax_version(), "syntax_version");
  ASSERT_EQ(parsed_expr.source_info().location(), "location");
  EXPECT_THAT(parsed_expr.source_info().line_offsets(),
              testing::UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(
      parsed_expr.source_info().positions(),
      testing::UnorderedElementsAre(testing::Pair(1, 1), testing::Pair(2, 2)));
}

TEST(AstTest, ListTypeMutableConstruction) {
  ListType type;
  type.mutable_elem_type() = Type(PrimitiveType::kBool);
  EXPECT_EQ(absl::get<PrimitiveType>(type.elem_type().type_kind()),
            PrimitiveType::kBool);
}

TEST(AstTest, MapTypeMutableConstruction) {
  MapType type;
  type.mutable_key_type() = Type(PrimitiveType::kBool);
  type.mutable_value_type() = Type(PrimitiveType::kBool);
  EXPECT_EQ(absl::get<PrimitiveType>(type.key_type().type_kind()),
            PrimitiveType::kBool);
  EXPECT_EQ(absl::get<PrimitiveType>(type.value_type().type_kind()),
            PrimitiveType::kBool);
}

TEST(AstTest, MapTypeComparatorKeyType) {
  MapType type;
  type.mutable_key_type() = Type(PrimitiveType::kBool);
  EXPECT_FALSE(type == MapType());
}

TEST(AstTest, MapTypeComparatorValueType) {
  MapType type;
  type.mutable_value_type() = Type(PrimitiveType::kBool);
  EXPECT_FALSE(type == MapType());
}

TEST(AstTest, FunctionTypeMutableConstruction) {
  FunctionType type;
  type.mutable_result_type() = Type(PrimitiveType::kBool);
  EXPECT_EQ(absl::get<PrimitiveType>(type.result_type().type_kind()),
            PrimitiveType::kBool);
}

TEST(AstTest, FunctionTypeComparatorArgTypes) {
  FunctionType type;
  type.mutable_arg_types().emplace_back(Type());
  EXPECT_FALSE(type == FunctionType());
}

TEST(AstTest, CheckedExpr) {
  CheckedExpr checked_expr;
  checked_expr.set_expr(Expr(1, Ident("name")));
  auto& source_info = checked_expr.mutable_source_info();
  source_info.set_syntax_version("syntax_version");
  source_info.set_location("location");
  source_info.set_line_offsets({1, 2, 3});
  source_info.set_positions({{1, 1}, {2, 2}});
  checked_expr.set_expr_version("expr_version");
  checked_expr.mutable_type_map().insert(
      {1, Type(PrimitiveType(PrimitiveType::kBool))});
  ASSERT_TRUE(absl::holds_alternative<Ident>(checked_expr.expr().expr_kind()));
  ASSERT_EQ(absl::get<Ident>(checked_expr.expr().expr_kind()).name(), "name");
  ASSERT_EQ(checked_expr.source_info().syntax_version(), "syntax_version");
  ASSERT_EQ(checked_expr.source_info().location(), "location");
  EXPECT_THAT(checked_expr.source_info().line_offsets(),
              testing::UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(
      checked_expr.source_info().positions(),
      testing::UnorderedElementsAre(testing::Pair(1, 1), testing::Pair(2, 2)));
  EXPECT_EQ(checked_expr.expr_version(), "expr_version");
}

TEST(AstTest, ListTypeDefaults) { EXPECT_EQ(ListType().elem_type(), Type()); }

TEST(AstTest, MapTypeDefaults) {
  EXPECT_EQ(MapType().key_type(), Type());
  EXPECT_EQ(MapType().value_type(), Type());
}

TEST(AstTest, FunctionTypeDefaults) {
  EXPECT_EQ(FunctionType().result_type(), Type());
}

TEST(AstTest, TypeDefaults) {
  EXPECT_EQ(Type().null(), nullptr);
  EXPECT_EQ(Type().primitive(), PrimitiveType::kPrimitiveTypeUnspecified);
  EXPECT_EQ(Type().wrapper(), PrimitiveType::kPrimitiveTypeUnspecified);
  EXPECT_EQ(Type().well_known(), WellKnownType::kWellKnownTypeUnspecified);
  EXPECT_EQ(Type().list_type(), ListType());
  EXPECT_EQ(Type().map_type(), MapType());
  EXPECT_EQ(Type().function(), FunctionType());
  EXPECT_EQ(Type().message_type(), MessageType());
  EXPECT_EQ(Type().type_param(), ParamType());
  EXPECT_EQ(Type().type(), Type());
  EXPECT_EQ(Type().error_type(), ErrorType());
  EXPECT_EQ(Type().abstract_type(), AbstractType());
}

TEST(AstTest, TypeComparatorTest) {
  Type type;
  type.set_type_kind(std::make_unique<Type>(PrimitiveType::kBool));
  EXPECT_FALSE(type.type() == Type());
}

TEST(AstTest, ExprMutableConstruction) {
  Expr expr;
  expr.mutable_const_expr().set_bool_value(true);
  ASSERT_TRUE(expr.has_const_expr());
  EXPECT_TRUE(expr.const_expr().bool_value());
  expr.mutable_ident_expr().set_name("expr");
  ASSERT_TRUE(expr.has_ident_expr());
  EXPECT_FALSE(expr.has_const_expr());
  EXPECT_EQ(expr.ident_expr().name(), "expr");
  expr.mutable_select_expr().set_field("field");
  ASSERT_TRUE(expr.has_select_expr());
  EXPECT_FALSE(expr.has_ident_expr());
  EXPECT_EQ(expr.select_expr().field(), "field");
  expr.mutable_call_expr().set_function("function");
  ASSERT_TRUE(expr.has_call_expr());
  EXPECT_FALSE(expr.has_select_expr());
  EXPECT_EQ(expr.call_expr().function(), "function");
  expr.mutable_list_expr();
  EXPECT_TRUE(expr.has_list_expr());
  EXPECT_FALSE(expr.has_call_expr());
  expr.mutable_struct_expr().set_message_name("name");
  ASSERT_TRUE(expr.has_struct_expr());
  EXPECT_EQ(expr.struct_expr().message_name(), "name");
  EXPECT_FALSE(expr.has_list_expr());
  expr.mutable_comprehension_expr().set_accu_var("accu_var");
  ASSERT_TRUE(expr.has_comprehension_expr());
  EXPECT_FALSE(expr.has_list_expr());
  EXPECT_EQ(expr.comprehension_expr().accu_var(), "accu_var");
}

TEST(AstTest, ReferenceConstantDefaultValue) {
  Reference reference;
  EXPECT_EQ(reference.value(), Constant());
}

TEST(AstTest, TypeCopyable) {
  Type type = Type(PrimitiveType::kBool);
  Type type2 = type;
  EXPECT_TRUE(type2.has_primitive());
  EXPECT_EQ(type2, type);

  type = Type(ListType(std::make_unique<Type>(PrimitiveType::kBool)));
  type2 = type;
  EXPECT_TRUE(type2.has_list_type());
  EXPECT_EQ(type2, type);

  type = Type(MapType(std::make_unique<Type>(PrimitiveType::kBool),
                      std::make_unique<Type>(PrimitiveType::kBool)));
  type2 = type;
  EXPECT_TRUE(type2.has_map_type());
  EXPECT_EQ(type2, type);

  type = Type(FunctionType(std::make_unique<Type>(PrimitiveType::kBool), {}));
  type2 = type;
  EXPECT_TRUE(type2.has_function());
  EXPECT_EQ(type2, type);

  type = Type(AbstractType("optional", {Type(PrimitiveType::kBool)}));
  type2 = type;
  EXPECT_TRUE(type2.has_abstract_type());
  EXPECT_EQ(type2, type);
}

TEST(AstTest, DeepCopyIsDeep) {
  Expr expr;
  auto& call = expr.mutable_call_expr();
  call.set_function("_[_]");
  auto& arg1 = call.mutable_args().emplace_back();
  arg1.mutable_ident_expr().set_name("x");
  auto& arg2 = call.mutable_args().emplace_back();
  arg2.mutable_const_expr().set_string_value("x");

  Expr expr2(expr.DeepCopy());

  EXPECT_EQ(expr2, expr);

  expr2.mutable_call_expr().mutable_args()[0].mutable_ident_expr().set_name(
      "y");
  expr2.mutable_call_expr()
      .mutable_args()[1]
      .mutable_const_expr()
      .set_string_value("y");

  EXPECT_NE(expr2, expr);
}

TEST(AstTest, TypeMoveable) {
  Type type = Type(PrimitiveType::kBool);
  Type type2 = type;
  Type type3 = std::move(type);
  EXPECT_TRUE(type2.has_primitive());
  EXPECT_EQ(type2, type3);

  type = Type(ListType(std::make_unique<Type>(PrimitiveType::kBool)));
  type2 = type;
  type3 = std::move(type);
  EXPECT_TRUE(type2.has_list_type());
  EXPECT_EQ(type2, type3);

  type = Type(MapType(std::make_unique<Type>(PrimitiveType::kBool),
                      std::make_unique<Type>(PrimitiveType::kBool)));
  type2 = type;
  type3 = std::move(type);
  EXPECT_TRUE(type2.has_map_type());
  EXPECT_EQ(type2, type3);

  type = Type(FunctionType(std::make_unique<Type>(PrimitiveType::kBool), {}));
  type2 = type;
  type3 = std::move(type);
  EXPECT_TRUE(type2.has_function());
  EXPECT_EQ(type2, type3);

  type = Type(AbstractType("optional", {Type(PrimitiveType::kBool)}));
  type2 = type;
  type3 = std::move(type);
  EXPECT_TRUE(type2.has_abstract_type());
  EXPECT_EQ(type2, type3);
}

TEST(AstTest, NestedTypeKindCopyAssignable) {
  ListType list_type(std::make_unique<Type>(PrimitiveType::kBool));
  ListType list_type2;
  list_type2 = list_type;

  EXPECT_EQ(list_type2, list_type);

  MapType map_type(std::make_unique<Type>(PrimitiveType::kBool),
                   std::make_unique<Type>(PrimitiveType::kBool));
  MapType map_type2;
  map_type2 = map_type;

  AbstractType abstract_type(
      "abstract", {Type(PrimitiveType::kBool), Type(PrimitiveType::kBool)});
  AbstractType abstract_type2;
  abstract_type2 = abstract_type;

  EXPECT_EQ(abstract_type2, abstract_type);

  FunctionType function_type(
      std::make_unique<Type>(PrimitiveType::kBool),
      {Type(PrimitiveType::kBool), Type(PrimitiveType::kBool)});
  FunctionType function_type2;
  function_type2 = function_type;

  EXPECT_EQ(function_type2, function_type);
}

TEST(AstTest, ExtensionSupported) {
  SourceInfo source_info;

  source_info.mutable_extensions().push_back(
      Extension("constant_folding", nullptr, {}));

  EXPECT_EQ(source_info.extensions()[0],
            Extension("constant_folding", nullptr, {}));
}

TEST(AstTest, ExtensionEquality) {
  Extension extension1("constant_folding", nullptr, {});

  EXPECT_EQ(extension1, Extension("constant_folding", nullptr, {}));

  EXPECT_NE(extension1,
            Extension("constant_folding",
                      std::make_unique<Extension::Version>(1, 0), {}));
  EXPECT_NE(extension1, Extension("constant_folding", nullptr,
                                  {Extension::Component::kRuntime}));

  EXPECT_EQ(extension1,
            Extension("constant_folding",
                      std::make_unique<Extension::Version>(0, 0), {}));
}

TEST(AstTest, ExtensionsCopyable) {
  SourceInfo source_info;

  source_info.mutable_extensions().push_back(Extension(
      "extension", std::make_unique<Extension::Version>(1, 2),
      {Extension::Component::kTypeChecker, Extension::Component::kRuntime}));

  SourceInfo source_info_copy = source_info.DeepCopy();

  EXPECT_EQ(source_info_copy, source_info);

  EXPECT_EQ(source_info_copy.extensions()[0],
            Extension("extension", std::make_unique<Extension::Version>(1, 2),
                      {Extension::Component::kTypeChecker,
                       Extension::Component::kRuntime}));
}

}  // namespace
}  // namespace ast_internal
}  // namespace cel
