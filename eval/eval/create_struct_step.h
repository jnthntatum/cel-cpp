// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_STRUCT_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_STRUCT_STEP_H_

#include <cstdint>
#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "base/ast_internal/expr.h"
#include "common/type_manager.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

// Creates an `ExpressionStep` which performs `CreateStruct` for a
// message/struct.
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateStructStepForStruct(
    const cel::ast_internal::CreateStruct& create_struct_expr, std::string name,
    int64_t expr_id, cel::TypeManager& type_manager);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_STRUCT_STEP_H_
