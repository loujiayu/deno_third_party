// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/func-name-inferrer.h"

#include "src/ast/ast-value-factory.h"
#include "src/ast/ast.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

FuncNameInferrer::FuncNameInferrer(AstValueFactory* ast_value_factory,
                                   Zone* zone)
    : ast_value_factory_(ast_value_factory),
      entries_stack_(zone),
      names_stack_(zone),
      funcs_to_infer_(zone),
      zone_(zone) {}

void FuncNameInferrer::PushEnclosingName(const AstRawString* name) {
  // Enclosing name is a name of a constructor function. To check
  // that it is really a constructor, we check that it is not empty
  // and starts with a capital letter.
  if (!name->IsEmpty() && unibrow::Uppercase::Is(name->FirstCharacter())) {
    names_stack_.push_back(Name(name, kEnclosingConstructorName));
  }
}


void FuncNameInferrer::PushLiteralName(const AstRawString* name) {
  if (IsOpen() && name != ast_value_factory_->prototype_string()) {
    names_stack_.push_back(Name(name, kLiteralName));
  }
}


void FuncNameInferrer::PushVariableName(const AstRawString* name) {
  if (IsOpen() && name != ast_value_factory_->dot_result_string()) {
    names_stack_.push_back(Name(name, kVariableName));
  }
}

void FuncNameInferrer::RemoveAsyncKeywordFromEnd() {
  if (IsOpen()) {
    CHECK_GT(names_stack_.size(), 0);
    CHECK(names_stack_.back().name()->IsOneByteEqualTo("async"));
    names_stack_.pop_back();
  }
}

void FuncNameInferrer::Leave() {
  DCHECK(IsOpen());
  size_t last_entry = entries_stack_.back();
  entries_stack_.pop_back();
  names_stack_.Rewind(last_entry);
  if (entries_stack_.is_empty()) funcs_to_infer_.Rewind();
}

const AstConsString* FuncNameInferrer::MakeNameFromStack() {
  AstConsString* result = ast_value_factory_->NewConsString();
  auto it = names_stack_.begin();
  while (it != names_stack_.end()) {
    // Advance the iterator to be able to peek the next value.
    auto current = it++;
    // Skip consecutive variable declarations.
    if (it != names_stack_.end() && current->type() == kVariableName &&
        it->type() == kVariableName) {
      continue;
    }
    // Add name. Separate names with ".".
    if (!result->IsEmpty()) {
      result->AddString(zone(), ast_value_factory_->dot_string());
    }
    result->AddString(zone(), current->name());
  }
  return result;
}

void FuncNameInferrer::InferFunctionsNames() {
  const AstConsString* func_name = MakeNameFromStack();
  for (FunctionLiteral* func : funcs_to_infer_) {
    func->set_raw_inferred_name(func_name);
  }
  funcs_to_infer_.Rewind(0);
}


}  // namespace internal
}  // namespace v8
