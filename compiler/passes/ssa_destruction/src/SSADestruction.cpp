#include "memoir/utility/FunctionNames.hpp"

#include "memoir/support/Assert.hpp"
#include "memoir/support/Casting.hpp"
#include "memoir/support/Print.hpp"

#include "SSADestruction.hpp"

namespace llvm::memoir {

SSADestructionVisitor::SSADestructionVisitor(llvm::noelle::DomTreeSummary &DT,
                                             LivenessAnalysis &LA,
                                             ValueNumbering &VN,
                                             SSADestructionStats *stats)
  : DT(DT),
    LA(LA),
    VN(VN),
    stats(stats) {
  // Do nothing.
}

void SSADestructionVisitor::visitInstruction(llvm::Instruction &I) {
  return;
}

void SSADestructionVisitor::visitUsePHIInst(UsePHIInst &I) {
  auto &used_collection = I.getUsedCollectionOperand();
  auto &collection = I.getCollectionValue();

  this->coalesce(collection, used_collection);

  this->markForCleanup(I);

  return;
}

void SSADestructionVisitor::visitDefPHIInst(DefPHIInst &I) {
  auto &defined_collection = I.getDefinedCollectionOperand();
  auto &collection = I.getCollectionValue();

  this->coalesce(collection, defined_collection);

  this->markForCleanup(I);

  return;
}

static void slice_to_view(SliceInst &I) {
  auto &call_inst = I.getCallInst();
  auto *view_func = FunctionNames::get_memoir_function(*call_inst.getModule(),
                                                       MemOIR_Func::VIEW);
  MEMOIR_NULL_CHECK(view_func, "Could not find the memoir view function");
  call_inst.setCalledFunction(view_func);
  return;
}

void SSADestructionVisitor::visitSliceInst(SliceInst &I) {
  auto &collection = I.getCollectionOperand();
  auto &slice = I.getSliceAsValue();

  // If the collection is dead immediately following,
  // then we can replace this slice with a view.
  if (!this->LA.is_live(collection, I)) {
    slice_to_view(I);
    return;
  }

  // If the slice is disjoint from the live slice range of the collection.
  bool is_disjoint = true;
  auto &slice_begin = I.getBeginIndex();
  auto &slice_end = I.getEndIndex();
  set<SliceInst *> slice_users = {};
  for (auto *user : collection.users()) {
    auto *user_as_inst = dyn_cast<llvm::Instruction>(user);
    if (!user_as_inst) {
      // This is an overly conservative check.
      is_disjoint = false;
      break;
    }

    auto *user_as_memoir = MemOIRInst::get(*user_as_inst);
    if (!user_as_memoir) {
      // Also overly conservative, we _can_ handle PHIs.
      is_disjoint = false;
      break;
    }

    if (auto *user_as_slice = dyn_cast<SliceInst>(user_as_memoir)) {
      // We will check all slice users for non-overlapping index spaces, _if_
      // there are no other users.
      slice_users.insert(user_as_slice);
      continue;
    }

    if (auto *user_as_access = dyn_cast<AccessInst>(user_as_memoir)) {
      // TODO: check the interval range of the index.
    }

    is_disjoint = false;
    break;
  }

  if (!is_disjoint) {
    return;
  }

  slice_users.erase(&I);

  // Check the slice users to see if they are non-overlapping.
  if (!slice_users.empty()) {
    auto &slice_begin = I.getBeginIndex();
    auto &slice_end = I.getEndIndex();
    set<SliceInst *> visited = {};
    list<llvm::Value *> limits = { &slice_begin, &slice_end };
    while (visited.size() < slice_users.size()) {
      bool found_new_limit = false;
      for (auto *user_as_slice : slice_users) {
        if (visited.find(user_as_slice) != visited.end()) {
          continue;
        }

        // Check if this slice range is overlapping.
        auto &user_slice_begin = user_as_slice->getBeginIndex();
        auto &user_slice_end = user_as_slice->getEndIndex();
        if (&user_slice_begin == limits.back()) {
          visited.insert(user_as_slice);
          limits.push_back(&user_slice_end);
          found_new_limit = true;
        } else if (&user_slice_end == limits.front()) {
          visited.insert(user_as_slice);
          limits.push_front(&user_slice_begin);
          found_new_limit = true;
        }
      }

      // If we found a new limit, continue working.
      if (!found_new_limit) {
        break;
      }
    }

    // Otherwise, we need to bring out the big guns and check for relations.
    auto *slice_begin_expr = this->VN.get(slice_begin);
    auto *slice_end_expr = this->VN.get(slice_end);
    ValueExpression *new_lower_limit = nullptr;
    ValueExpression *new_upper_limit = nullptr;
    for (auto *user_as_slice : slice_users) {
      if (visited.find(user_as_slice) != visited.end()) {
        continue;
      }

      // Check if this slice range is non-overlapping, with an offset.
      auto *user_slice_begin_expr =
          this->VN.get(user_as_slice->getBeginIndex());
      MEMOIR_NULL_CHECK(user_slice_begin_expr,
                        "Error making value expression for begin index");
      auto *user_slice_end_expr = this->VN.get(user_as_slice->getEndIndex());
      MEMOIR_NULL_CHECK(user_slice_end_expr,
                        "Error making value expression for end index");

      if (*user_slice_end_expr < *slice_begin_expr
          || *user_slice_begin_expr > *slice_end_expr) {
        continue;
      } else {
        warnln("Big guns failed,"
               " open an issue if this shouldn't have happened.");
        is_disjoint = false;
      }
    }
  }

  if (is_disjoint) {
    slice_to_view(I);
  }

  return;
}

void SSADestructionVisitor::visitJoinInst(JoinInst &I) {
  auto &collection = I.getCollectionAsValue();
  auto num_joined = I.getNumberOfJoins();

  // For each join operand, if it is dead after the join, we can coallesce this
  // join with it.
  bool all_dead = true;
  for (auto join_idx = 0; join_idx < num_joined; join_idx++) {
    auto &joined_collection = I.getJoinedOperand(join_idx);
    if (this->LA.is_live(joined_collection, I)) {
      all_dead = false;
      break;
    }
  }

  // If all join operands are dead, convert the join to a series of appends.
  if (all_dead) {
    auto &first_collection = I.getJoinedOperand(0);
    MemOIRBuilder builder(I);
    for (auto join_idx = 1; join_idx < num_joined; join_idx++) {
      auto &joined_collection = I.getJoinedOperand(join_idx);
      println("Creating append");
      println("  ", first_collection);
      println("  ", joined_collection);

      builder.CreateSeqAppendInst(&first_collection, &joined_collection);
    }

    // Coalesce the first operand and the join.
    this->coalesce(I, first_collection);

    // The join is dead after coalescence.
    this->markForCleanup(I);

    return;
  }

  // If all operands of the join are views of the same collection:
  //  - If views are in order, coallesce resultant and the viewed collection.
  //  - Otherwise, determine if the size is the same after the join:
  //     - If the size is the same, convert to a swap.
  //     - Otherwise, convert to a remove.

  // Otherwise, some operands are views from a different collection:
  //  - If the size is the same, convert to a swap.
  //  - Otherwise, convert to an append.

  return;
}

void SSADestructionVisitor::cleanup() {
  for (auto *inst : instructions_to_delete) {
    println(*inst);
    inst->eraseFromParent();
  }
}

void SSADestructionVisitor::coalesce(MemOIRInst &I, llvm::Value &replacement) {
  this->coalesce(I.getCallInst(), replacement);
}

void SSADestructionVisitor::coalesce(llvm::Value &V, llvm::Value &replacement) {
  println("Coalesce:");
  println("  ", V);
  println("  ", replacement);
  this->coalesced_values[&V] = &replacement;
}

llvm::Value *SSADestructionVisitor::find_replacement(llvm::Value *value) {
  auto *replacement_value = value;
  auto found = this->replaced_values.find(value);
  while (found != this->replaced_values.end()) {
    replacement_value = found->second;
    found = this->replaced_values.find(replacement_value);
  }
  return replacement_value;
}

void SSADestructionVisitor::do_coalesce(llvm::Value &V) {
  auto found_coalesce = this->coalesced_values.find(&V);
  if (found_coalesce == this->coalesced_values.end()) {
    return;
  }

  auto *replacement = this->find_replacement(found_coalesce->second);

  println("Coalescing:");
  println("  ", V);
  println("  ", *replacement);

  V.replaceAllUsesWith(replacement);

  this->replaced_values[&V] = replacement;
}

void SSADestructionVisitor::markForCleanup(MemOIRInst &I) {
  this->markForCleanup(I.getCallInst());
}

void SSADestructionVisitor::markForCleanup(llvm::Instruction &I) {
  this->instructions_to_delete.insert(&I);
}

} // namespace llvm::memoir
