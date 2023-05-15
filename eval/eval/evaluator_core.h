#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_CORE_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_CORE_H_

#include <stddef.h>
#include <stdint.h>

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/ast_internal.h"
#include "base/handle.h"
#include "base/memory.h"
#include "base/type_manager.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/attribute_utility.h"
#include "eval/eval/evaluator_stack.h"
#include "eval/internal/adapter_activation_impl.h"
#include "eval/internal/interop.h"
#include "eval/public/base_activation.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_type_registry.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_attribute_set.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/rtti.h"
#include "runtime/activation_interface.h"
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {

// Forward declaration of ExecutionFrame, to resolve circular dependency.
class ExecutionFrame;

using Expr = ::google::api::expr::v1alpha1::Expr;

// Class Expression represents single execution step.
class ExpressionStep {
 public:
  virtual ~ExpressionStep() = default;

  // Performs actual evaluation.
  // Values are passed between Expression objects via EvaluatorStack, which is
  // supplied with context.
  // Also, Expression gets values supplied by caller though Activation
  // interface.
  // ExpressionStep instances can in specific cases
  // modify execution order(perform jumps).
  virtual absl::Status Evaluate(ExecutionFrame* context) const = 0;

  // Returns corresponding expression object ID.
  // Requires that the input expression has IDs assigned to sub-expressions,
  // e.g. via a checker. The default value 0 is returned if there is no
  // expression associated (e.g. a jump step), or if there is no ID assigned to
  // the corresponding expression. Useful for error scenarios where information
  // from Expr object is needed to create CelError.
  virtual int64_t id() const = 0;

  // Returns if the execution step comes from AST.
  virtual bool ComesFromAst() const = 0;

  // Return the type of the underlying expression step for special handling in
  // the planning phase. This should only be overridden by special cases, and
  // callers must not make any assumptions about the default case.
  virtual cel::internal::TypeInfo TypeId() const = 0;
};

using ExecutionPath = std::vector<std::unique_ptr<const ExpressionStep>>;
using ExecutionPathView =
    absl::Span<const std::unique_ptr<const ExpressionStep>>;

class CelExpressionFlatEvaluationState : public CelEvaluationState {
 public:
  CelExpressionFlatEvaluationState(size_t value_stack_size,
                                   google::protobuf::Arena* arena);

  struct ComprehensionVarEntry {
    absl::string_view name;
    // present if we're in part of the loop context where this can be accessed.
    cel::Handle<cel::Value> value;
    AttributeTrail attr_trail;
  };

  struct IterFrame {
    ComprehensionVarEntry iter_var;
    ComprehensionVarEntry accu_var;
  };

  void Reset();

  EvaluatorStack& value_stack() { return value_stack_; }

  std::vector<IterFrame>& iter_stack() { return iter_stack_; }

  IterFrame& IterStackTop() { return iter_stack_[iter_stack().size() - 1]; }

  google::protobuf::Arena* arena() { return memory_manager_.arena(); }

  cel::MemoryManager& memory_manager() { return memory_manager_; }

  cel::TypeFactory& type_factory() { return type_factory_; }

  cel::TypeManager& type_manager() { return type_manager_; }

  cel::ValueFactory& value_factory() { return value_factory_; }

 private:
  // TODO(issues/5): State owns a ProtoMemoryManager to adapt from the client
  // provided arena. In the future, clients will have to maintain the particular
  // manager they want to use for evaluation.
  cel::extensions::ProtoMemoryManager memory_manager_;
  EvaluatorStack value_stack_;
  std::vector<IterFrame> iter_stack_;
  cel::TypeFactory type_factory_;
  cel::TypeManager type_manager_;
  cel::ValueFactory value_factory_;
};

// ExecutionFrame provides context for expression evaluation.
// The lifecycle of the object is bound to CelExpression Evaluate(...) call.
class ExecutionFrame {
 public:
  // flat is the flattened sequence of execution steps that will be evaluated.
  // activation provides bindings between parameter names and values.
  // arena serves as allocation manager during the expression evaluation.

  ExecutionFrame(ExecutionPathView flat, const BaseActivation& activation,
                 const CelTypeRegistry* type_registry,
                 const cel::RuntimeOptions& options,
                 CelExpressionFlatEvaluationState* state)
      : pc_(0UL),
        execution_path_(flat),
        activation_(activation),
        modern_activation_(activation),
        type_registry_(*type_registry),
        options_(options),
        attribute_utility_(modern_activation_.GetUnknownAttributes(),
                           modern_activation_.GetMissingAttributes(),
                           state->memory_manager()),
        max_iterations_(options_.comprehension_max_iterations),
        iterations_(0),
        state_(state) {}

  // Returns next expression to evaluate.
  const ExpressionStep* Next();

  // Evaluate the execution frame to completion.
  absl::StatusOr<cel::Handle<cel::Value>> Evaluate(
      const CelEvaluationListener& listener);

  // Intended for use only in conditionals.
  absl::Status JumpTo(int offset) {
    int new_pc = static_cast<int>(pc_) + offset;
    if (new_pc < 0 || new_pc > static_cast<int>(execution_path_.size())) {
      return absl::Status(absl::StatusCode::kInternal,
                          absl::StrCat("Jump address out of range: position: ",
                                       pc_, ",offset: ", offset,
                                       ", range: ", execution_path_.size()));
    }
    pc_ = static_cast<size_t>(new_pc);
    return absl::OkStatus();
  }

  EvaluatorStack& value_stack() { return state_->value_stack(); }

  bool enable_unknowns() const {
    return options_.unknown_processing !=
           cel::UnknownProcessingOptions::kDisabled;
  }

  bool enable_unknown_function_results() const {
    return options_.unknown_processing ==
           cel::UnknownProcessingOptions::kAttributeAndFunction;
  }

  bool enable_missing_attribute_errors() const {
    return options_.enable_missing_attribute_errors;
  }

  bool enable_heterogeneous_numeric_lookups() const {
    return options_.enable_heterogeneous_equality;
  }

  cel::MemoryManager& memory_manager() { return state_->memory_manager(); }

  cel::TypeFactory& type_factory() { return state_->type_factory(); }

  cel::TypeManager& type_manager() { return state_->type_manager(); }

  cel::ValueFactory& value_factory() { return state_->value_factory(); }

  const CelTypeRegistry& type_registry() { return type_registry_; }

  const AttributeUtility& attribute_utility() const {
    return attribute_utility_;
  }

  // Returns reference to Activation
  const BaseActivation& activation() const { return activation_; }

  // Returns reference to the modern API activation.
  const cel::ActivationInterface& modern_activation() const {
    return modern_activation_;
  }

  // Creates a new frame for the iteration variables identified by iter_var_name
  // and accu_var_name.
  absl::Status PushIterFrame(absl::string_view iter_var_name,
                             absl::string_view accu_var_name);

  // Discards the top frame for iteration variables.
  absl::Status PopIterFrame();

  // Sets the value of the accumuation variable
  absl::Status SetAccuVar(cel::Handle<cel::Value> value);

  // Sets the value of the accumulation variable
  absl::Status SetAccuVar(cel::Handle<cel::Value> value, AttributeTrail trail);

  // Sets the value of the iteration variable
  absl::Status SetIterVar(cel::Handle<cel::Value> value);

  // Sets the value of the iteration variable
  absl::Status SetIterVar(cel::Handle<cel::Value> value, AttributeTrail trail);

  // Clears the value of the iteration variable
  absl::Status ClearIterVar();

  // Gets the current value of either an iteration variable or accumulation
  // variable.
  // Returns false if the variable is not yet set or has been cleared.
  bool GetIterVar(absl::string_view name, cel::Handle<cel::Value>* value,
                  AttributeTrail* trail) const;

  // Increment iterations and return an error if the iteration budget is
  // exceeded
  absl::Status IncrementIterations() {
    if (max_iterations_ == 0) {
      return absl::OkStatus();
    }
    iterations_++;
    if (iterations_ >= max_iterations_) {
      return absl::Status(absl::StatusCode::kInternal,
                          "Iteration budget exceeded");
    }
    return absl::OkStatus();
  }

 private:
  size_t pc_;  // pc_ - Program Counter. Current position on execution path.
  ExecutionPathView execution_path_;
  const BaseActivation& activation_;
  cel::interop_internal::AdapterActivationImpl modern_activation_;
  const CelTypeRegistry& type_registry_;
  const cel::RuntimeOptions& options_;  // owned by the FlatExpr instance
  AttributeUtility attribute_utility_;
  const int max_iterations_;
  int iterations_;
  CelExpressionFlatEvaluationState* state_;
};

// Implementation of the CelExpression that utilizes flattening
// of the expression tree.
class CelExpressionFlatImpl : public CelExpression {
 public:
  // Constructs CelExpressionFlatImpl instance.
  // path is flat execution path that is based upon
  // flattened AST tree. Max iterations dictates the maximum number of
  // iterations in the comprehension expressions (use 0 to disable the upper
  // bound).
  CelExpressionFlatImpl(ExecutionPath path,
                        const CelTypeRegistry* type_registry,
                        const cel::RuntimeOptions& options)
      : path_(std::move(path)),
        type_registry_(*type_registry),
        options_(options) {}

  // Move-only
  CelExpressionFlatImpl(const CelExpressionFlatImpl&) = delete;
  CelExpressionFlatImpl& operator=(const CelExpressionFlatImpl&) = delete;

  std::unique_ptr<CelEvaluationState> InitializeState(
      google::protobuf::Arena* arena) const override;

  // Implementation of CelExpression evaluate method.
  absl::StatusOr<CelValue> Evaluate(const BaseActivation& activation,
                                    google::protobuf::Arena* arena) const override {
    return Evaluate(activation, InitializeState(arena).get());
  }

  absl::StatusOr<CelValue> Evaluate(const BaseActivation& activation,
                                    CelEvaluationState* state) const override;

  // Implementation of CelExpression trace method.
  absl::StatusOr<CelValue> Trace(
      const BaseActivation& activation, google::protobuf::Arena* arena,
      CelEvaluationListener callback) const override {
    return Trace(activation, InitializeState(arena).get(), callback);
  }

  absl::StatusOr<CelValue> Trace(const BaseActivation& activation,
                                 CelEvaluationState* state,
                                 CelEvaluationListener callback) const override;

 private:
  const ExecutionPath path_;
  const CelTypeRegistry& type_registry_;
  cel::RuntimeOptions options_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_EVALUATOR_CORE_H_
