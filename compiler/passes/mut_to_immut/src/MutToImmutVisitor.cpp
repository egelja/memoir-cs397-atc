#include "memoir/support/Assert.hpp"
#include "memoir/support/Casting.hpp"
#include "memoir/support/Print.hpp"

#include "MutToImmutVisitor.hpp"

namespace llvm::memoir {

llvm::Value *MutToImmutVisitor::update_reaching_definition(
    llvm::Value *variable,
    llvm::Instruction *program_point) {
  // Search through the chain of definitions for variable until we find the
  // closest definition that dominates the program point. Then update the
  // reaching definition.
  auto *reaching_variable = variable;

  println("Computing reaching definition:");
  println("  for", *variable);
  println("  at ", *program_point);

  // println(*(program_point->getParent()));

  do {
    auto found_reaching_definition =
        this->reaching_definitions.find(reaching_variable);
    if (found_reaching_definition == this->reaching_definitions.end()) {
      reaching_variable = nullptr;
      break;
    }

    auto *next_reaching_variable = found_reaching_definition->second;

    if (next_reaching_variable) {
      println("=> ", *next_reaching_variable);
    } else {
      println("=> NULL");
    }
    reaching_variable = next_reaching_variable;

    // If the reaching definition dominates the program point, update it.
    if (auto *reaching_definition =
            dyn_cast_or_null<llvm::Instruction>(reaching_variable)) {
      if (this->DT.dominates(reaching_definition, program_point)) {
        break;
      }
    }

    // Arguments dominate all program points in the function.
    if (auto *reaching_definition_as_argument =
            dyn_cast_or_null<llvm::Argument>(reaching_variable)) {
      break;
    }

  } while (reaching_variable != nullptr && reaching_variable != variable);

  this->reaching_definitions[variable] = reaching_variable;

  return reaching_variable;
}

MutToImmutVisitor::MutToImmutVisitor(
    llvm::noelle::DomTreeSummary &DT,
    ordered_set<llvm::Value *> memoir_names,
    map<llvm::PHINode *, llvm::Value *> inserted_phis)
  : DT(DT),
    inserted_phis(inserted_phis) {
  this->reaching_definitions = {};
  for (auto *name : memoir_names) {
    this->reaching_definitions[name] = name;
  }
}

void MutToImmutVisitor::visitInstruction(llvm::Instruction &I) {
  for (auto &operand_use : I.operands()) {
    auto *operand_value = operand_use.get();
    if (!Type::value_is_collection_type(*operand_value)) {
      continue;
    }

    auto *reaching_operand = update_reaching_definition(operand_value, &I);
    operand_use.set(reaching_operand);
  }

  if (Type::value_is_collection_type(I)) {
    this->reaching_definitions[&I] = &I;
  }

  return;
}

void MutToImmutVisitor::visitPHINode(llvm::PHINode &I) {
  auto found_inserted_phi = this->inserted_phis.find(&I);
  if (found_inserted_phi != this->inserted_phis.end()) {
    auto *named_variable = found_inserted_phi->second;
    auto *reaching_definition =
        this->update_reaching_definition(named_variable, &I);
    this->reaching_definitions[&I] = reaching_definition;
    this->reaching_definitions[named_variable] = &I;
  } else {
    this->reaching_definitions[&I] = &I;
  }

  return;
}

void MutToImmutVisitor::visitSeqInsertInst(SeqInsertInst &I) {
  MemOIRBuilder builder(I);

  auto &collection_type = I.getCollection().getType();
  auto *collection_orig = &I.getCollectionOperand();
  auto *collection_value =
      update_reaching_definition(collection_orig, &I.getCallInst());
  auto *write_value = &I.getValueInserted();
  auto *index_value = &I.getIndex();

  auto *elem_alloc =
      &builder.CreateSequenceAllocInst(collection_type, 1, "insert.elem.")
           ->getCallInst();
  auto *elem_write =
      &builder
           .CreateIndexWriteInst(collection_type.getElementType(),
                                 write_value,
                                 collection_value,
                                 index_value,
                                 "insert.elem.value.")
           ->getCallInst();

  if (auto *index_as_const_int = dyn_cast<llvm::ConstantInt>(index_value)) {
    if (index_as_const_int->isZero()) {
      // We only need to create a join to the front.
      auto *push_front_join = &builder
                                   .CreateJoinInst(vector<llvm::Value *>(
                                       { collection_value, elem_alloc }))
                                   ->getCallInst();

      this->reaching_definitions[collection_orig] = push_front_join;
      this->reaching_definitions[push_front_join] = collection_value;

      this->instructions_to_delete.insert(&I.getCallInst());
      return;
    }
  }

  // if (auto *index_as_size = dyn_cast<SizeInst>(&index_value)) {
  // TODO: if the index is a size(collection) and the collection has not been
  // modified in size since the call to size(collection), we can simply join
  // to the end.
  // }

  auto *left_slice = &builder
                          .CreateSliceInst(collection_value,
                                           (int64_t)0,
                                           index_value,
                                           "insert.left.")
                          ->getCallInst();
  auto *right_slice =
      &builder
           .CreateSliceInst(collection_value, index_value, -1, "insert.right.")
           ->getCallInst();

  auto *insert_join =
      &builder
           .CreateJoinInst(
               vector<llvm::Value *>({ left_slice, elem_alloc, right_slice }),
               "insert.join.")
           ->getCallInst();

  this->reaching_definitions[collection_orig] = insert_join;
  this->reaching_definitions[insert_join] = collection_value;

  this->instructions_to_delete.insert(&I.getCallInst());

  return;
}

void MutToImmutVisitor::visitSeqRemoveInst(SeqRemoveInst &I) {
  MemOIRBuilder builder(I);

  auto *collection = &I.getCollection();
  auto *collection_type = &I.getCollection().getType();
  auto *collection_orig = &I.getCollectionOperand();
  auto *collection_value =
      update_reaching_definition(collection_orig, &I.getCallInst());
  auto *begin_value = &I.getBeginIndex();
  auto *end_value = &I.getEndIndex();

  if (auto *begin_as_const_int = dyn_cast<llvm::ConstantInt>(begin_value)) {
    if (begin_as_const_int->isZero()) {
      // We only need to create a slice from [end_index:end)
      auto *pop_front =
          &builder
               .CreateSliceInst(collection_value, end_value, -1, "remove.rest.")
               ->getCallInst();

      this->reaching_definitions[collection_orig] = pop_front;
      this->reaching_definitions[pop_front] = collection_value;

      this->instructions_to_delete.insert(&I.getCallInst());

      return;
    }
  }

  // if (auto *index_as_size = dyn_cast<SizeInst>(&index_value)) {
  // TODO: if the index is a size(collection) and the collection has not been
  // modified in size since the call to size(collection), we can simply slice
  // from [0,begin)
  // }

  auto *left_slice = &builder
                          .CreateSliceInst(collection_value,
                                           (int64_t)0,
                                           begin_value,
                                           "remove.left.")
                          ->getCallInst();
  auto *right_slice =
      &builder
           .CreateSliceInst(collection_value, end_value, -1, "remove.right.")
           ->getCallInst();

  auto *remove_join =
      &builder
           .CreateJoinInst(vector<llvm::Value *>({ left_slice, right_slice }),
                           "remove.join.")
           ->getCallInst();
  // TODO: attach metadata to this to say that begin < end

  this->reaching_definitions[collection_orig] = remove_join;
  this->reaching_definitions[remove_join] = collection_value;

  this->instructions_to_delete.insert(&I.getCallInst());
  return;
}

void MutToImmutVisitor::visitSeqAppendInst(SeqAppendInst &I) {
  MemOIRBuilder builder(I);

  auto *collection_orig = &I.getCollectionOperand();
  auto *collection_value =
      update_reaching_definition(collection_orig, &I.getCallInst());
  auto *appended_collection_orig = &I.getAppendedCollectionOperand();
  auto *appended_collection_value =
      update_reaching_definition(appended_collection_orig, &I.getCallInst());

  auto *append_join =
      &builder
           .CreateJoinInst(vector<llvm::Value *>(
                               { collection_value, appended_collection_value }),
                           "append.")
           ->getCallInst();

  this->reaching_definitions[collection_orig] = append_join;
  this->reaching_definitions[append_join] = collection_value;

  this->instructions_to_delete.insert(&I.getCallInst());
  return;
}

void MutToImmutVisitor::visitSeqSwapInst(SeqSwapInst &I) {
  MemOIRBuilder builder(I);

  auto *from_collection_orig = &I.getFromCollectionOperand();
  auto *from_collection_value =
      update_reaching_definition(from_collection_orig, &I.getCallInst());
  auto *from_begin_value = &I.getBeginIndex();
  auto *from_end_value = &I.getEndIndex();
  auto *to_collection_orig = &I.getToCollectionOperand();
  auto *to_collection_value =
      update_reaching_definition(to_collection_orig, &I.getCallInst());
  auto *to_begin_value = &I.getToBeginIndex();

  llvm::Value *from_size = nullptr, *to_end_value = nullptr;
  llvm::Value *from_left = nullptr, *from_swap = nullptr, *from_right = nullptr;
  llvm::Value *to_left = nullptr, *to_swap = nullptr, *to_right = nullptr;
  if (from_collection_value == to_collection_value) {
    if (auto *from_begin_as_const_int =
            dyn_cast<llvm::ConstantInt>(from_begin_value)) {
      if (from_begin_as_const_int->isZero()) {
        from_size = from_end_value;
        from_swap = &builder
                         .CreateSliceInst(from_collection_value,
                                          (int64_t)0,
                                          from_end_value,
                                          "swap.from.")
                         ->getCallInst();
      }
    }

    if (from_swap == nullptr) {
      from_size =
          builder.CreateSub(from_end_value, from_begin_value, "swap.size.");
      from_left = &builder
                       .CreateSliceInst(from_collection_value,
                                        (int64_t)0,
                                        from_begin_value,
                                        "swap.from.")
                       ->getCallInst();
      from_swap = &builder
                       .CreateSliceInst(from_collection_value,
                                        from_begin_value,
                                        from_end_value,
                                        "swap.from.")
                       ->getCallInst();
    }

    to_end_value = builder.CreateAdd(to_begin_value, from_size, "swap.to.end.");

    if (from_end_value == to_begin_value) {
      to_swap = &builder
                     .CreateSliceInst(from_collection_value,
                                      to_begin_value,
                                      to_end_value,
                                      "swap.to.")
                     ->getCallInst();
      to_right = &builder
                      .CreateSliceInst(from_collection_value,
                                       to_end_value,
                                       -1,
                                       "swap.to.right")
                      ->getCallInst();
    } else {
      to_left = &builder
                     .CreateSliceInst(from_collection_value,
                                      from_end_value,
                                      to_begin_value,
                                      "swap.to.left")
                     ->getCallInst();
      to_swap = &builder
                     .CreateSliceInst(from_collection_value,
                                      to_begin_value,
                                      to_end_value,
                                      "swap.to.")
                     ->getCallInst();
      to_right = &builder
                      .CreateSliceInst(from_collection_value,
                                       to_end_value,
                                       -1,
                                       "swap.to.right")
                      ->getCallInst();
    }

    vector<llvm::Value *> collections_to_join;
    collections_to_join.reserve(6);
    if (from_left != nullptr) {
      collections_to_join.push_back(from_left);
    }
    collections_to_join.push_back(to_swap);
    if (from_right != nullptr) {
      collections_to_join.push_back(from_right);
    }
    if (to_left != nullptr) {
      collections_to_join.push_back(to_left);
    }
    collections_to_join.push_back(from_swap);
    if (to_right != nullptr) {
      collections_to_join.push_back(to_right);
    }

    llvm::Value *join =
        &builder.CreateJoinInst(collections_to_join, "swap.join.")
             ->getCallInst();

    this->reaching_definitions[from_collection_orig] = join;
    this->reaching_definitions[join] = from_collection_value;

    this->instructions_to_delete.insert(&I.getCallInst());
    return;
  }

  if (auto *from_begin_as_const_int =
          dyn_cast<llvm::ConstantInt>(from_begin_value)) {
    if (from_begin_as_const_int->isZero()) {
      // We only need to create a slice from [end_index:end)
      from_size = from_end_value;
      from_swap = &builder
                       .CreateSliceInst(from_collection_value,
                                        (int64_t)0,
                                        from_end_value,
                                        "swap.from.")
                       ->getCallInst();
      from_right = &builder
                        .CreateSliceInst(from_collection_value,
                                         from_end_value,
                                         -1,
                                         "swap.from.rest.")
                        ->getCallInst();
    }
  }

  if (from_swap == nullptr) {
    from_size =
        builder.CreateSub(from_end_value, from_begin_value, "swap.from.size.");
    from_left = &builder
                     .CreateSliceInst(from_collection_value,
                                      (int64_t)0,
                                      from_begin_value,
                                      "swap.from.left.")
                     ->getCallInst();
    from_swap = &builder
                     .CreateSliceInst(from_collection_value,
                                      from_begin_value,
                                      from_end_value,
                                      "swap.from.")
                     ->getCallInst();
    from_right = &builder
                      .CreateSliceInst(from_collection_value,
                                       from_end_value,
                                       -1,
                                       "swap.from.right.")
                      ->getCallInst();
  }

  if (auto *to_begin_as_const_int =
          dyn_cast<llvm::ConstantInt>(to_begin_value)) {
    if (to_begin_as_const_int->isZero()) {
      to_end_value = from_size;
      to_swap = &builder
                     .CreateSliceInst(to_collection_value,
                                      (int64_t)0,
                                      to_end_value,
                                      "swap.to.")
                     ->getCallInst();
      to_right = &builder
                      .CreateSliceInst(to_collection_value,
                                       to_end_value,
                                       -1,
                                       "swap.to.rest.")
                      ->getCallInst();
    }
  }

  if (to_swap == nullptr) {
    to_end_value = builder.CreateAdd(to_begin_value, from_size, "swap.to.end.");
    to_left = &builder
                   .CreateSliceInst(to_collection_value,
                                    (int64_t)0,
                                    to_begin_value,
                                    "swap.to.left.")
                   ->getCallInst();
    to_swap = &builder
                   .CreateSliceInst(to_collection_value,
                                    to_begin_value,
                                    to_end_value,
                                    "swap.to.")
                   ->getCallInst();
    to_right = &builder
                    .CreateSliceInst(to_collection_value,
                                     to_end_value,
                                     -1,
                                     "swap.to.right.")
                    ->getCallInst();
  }

  vector<llvm::Value *> from_incoming;
  from_incoming.reserve(3);
  if (from_left != nullptr) {
    from_incoming.push_back(from_left);
  }
  from_incoming.push_back(to_swap);
  if (from_right != nullptr) {
    from_incoming.push_back(from_right);
  }

  llvm::Value *from_join =
      &builder.CreateJoinInst(from_incoming, "swap.from.join.")->getCallInst();

  vector<llvm::Value *> to_incoming;
  to_incoming.reserve(3);
  if (to_left != nullptr) {
    to_incoming.push_back(to_left);
  }
  to_incoming.push_back(from_swap);
  if (to_right != nullptr) {
    to_incoming.push_back(to_right);
  }
  llvm::Value *to_join =
      &builder.CreateJoinInst(to_incoming, "swap.to.join.")->getCallInst();

  this->reaching_definitions[from_collection_orig] = from_join;
  this->reaching_definitions[from_join] = from_collection_value;
  this->reaching_definitions[to_collection_orig] = to_join;
  this->reaching_definitions[to_join] = to_collection_value;

  this->instructions_to_delete.insert(&I.getCallInst());
  return;
}

void MutToImmutVisitor::visitSeqSplitInst(SeqSplitInst &I) {
  MemOIRBuilder builder(I);

  auto *split_value = &I.getSplitValue();
  auto *collection_orig = &I.getCollectionOperand();
  auto *collection_value =
      update_reaching_definition(collection_orig, &I.getCallInst());
  auto *begin_value = &I.getBeginIndex();
  auto *end_value = &I.getEndIndex();

  if (auto *begin_as_const_int = dyn_cast<llvm::ConstantInt>(begin_value)) {
    if (begin_as_const_int->isZero()) {
      auto *split = &builder
                         .CreateSliceInst(collection_value,
                                          (int64_t)0,
                                          end_value,
                                          "split.")
                         ->getCallInst();
      auto *remaining = &builder
                             .CreateSliceInst(collection_value,
                                              end_value,
                                              -1,
                                              "split.remaining.")
                             ->getCallInst();

      this->reaching_definitions[split_value] = split;
      this->reaching_definitions[remaining] = collection_value;
      this->reaching_definitions[collection_orig] = remaining;

      this->instructions_to_delete.insert(&I.getCallInst());

      return;
    }
  }

  auto *left = &builder
                    .CreateSliceInst(collection_value,
                                     (int64_t)0,
                                     begin_value,
                                     "split.left.")
                    ->getCallInst();
  auto *split =
      &builder
           .CreateSliceInst(collection_value, begin_value, end_value, "split.")
           ->getCallInst();
  auto *right =
      &builder.CreateSliceInst(collection_value, end_value, -1, "split.right.")
           ->getCallInst();

  auto *remaining = &builder
                         .CreateJoinInst(vector<llvm::Value *>({ left, right }),
                                         "split.remaining.")
                         ->getCallInst();

  this->reaching_definitions[split_value] = split;
  this->reaching_definitions[remaining] = collection_value;
  this->reaching_definitions[collection_orig] = remaining;

  this->instructions_to_delete.insert(&I.getCallInst());
  return;
}

void MutToImmutVisitor::cleanup() {
  for (auto *inst : instructions_to_delete) {
    println(*inst);
    inst->eraseFromParent();
  }
}

} // namespace llvm::memoir
