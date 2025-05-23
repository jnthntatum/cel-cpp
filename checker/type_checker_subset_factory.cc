// Copyright 2025 Google LLC
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

#include "checker/type_checker_subset_factory.h"

#include <string>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "checker/type_checker_builder.h"

namespace cel {

TypeCheckerSubset::FunctionPredicate IncludeOverloadsByIdPredicate(
    absl::flat_hash_set<std::string> overload_ids) {
  return [overload_ids = std::move(overload_ids)](
             absl::string_view /*function*/, absl::string_view overload_id) {
    return overload_ids.contains(overload_id);
  };
}

TypeCheckerSubset::FunctionPredicate IncludeOverloadsByIdPredicate(
    absl::Span<const absl::string_view> overload_ids) {
  return IncludeOverloadsByIdPredicate(absl::flat_hash_set<std::string>(
      overload_ids.begin(), overload_ids.end()));
}

TypeCheckerSubset::FunctionPredicate ExcludeOverloadsByIdPredicate(
    absl::flat_hash_set<std::string> overload_ids) {
  return [overload_ids = std::move(overload_ids)](
             absl::string_view /*function*/, absl::string_view overload_id) {
    return !overload_ids.contains(overload_id);
  };
}

TypeCheckerSubset::FunctionPredicate ExcludeOverloadsByIdPredicate(
    absl::Span<const absl::string_view> overload_ids) {
  return ExcludeOverloadsByIdPredicate(absl::flat_hash_set<std::string>(
      overload_ids.begin(), overload_ids.end()));
}

}  // namespace cel
