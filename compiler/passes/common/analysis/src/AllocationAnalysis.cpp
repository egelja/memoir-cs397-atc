#include "common/analysis/AllocationAnalysis.hpp"

namespace llvm::memoir {

AllocationSummary::AllocationSummary(Module &M) : M(M) {
  for (auto &F : M) {
    for (auto &BB : F) {
      for (auto &I : BB) {
        auto call_inst = dyn_cast<llvm::CallInst>(&I);
        if (!call_inst) {
          continue;
        }

        /*
         * Build the AllocationSummary for this call instruction.
         */
        this->getAllocationSummary(*call_inst);
      }
    }
  }
}

AllocationSummary *AllocationAnalysis::getAllocationSummary(
    CallInst &call_inst) {
  /*
   * Look up the call instruction to see if we have a
   *   memoized AllocationSummary.
   */
  auto found_summary = this->allocation_summaries.find(&call_inst);
  if (found_summary != this->allocation_summaries.end()) {
    return found_summary.second;
  }

  /*
   * If the call instruction is not memoized, then
   *   we need to create its AllocationSummary.
   */
  auto callee = call_inst.getCalledFunction();

  /*
   * If the callee is an indirect call, then return a nullptr.
   * We don't handle indirect calls at the moment as they should
   *   be statically resolved.
   */
  if (callee == nullptr) {
    return nullptr;
  }

  auto callee_name = callee->getName();
  auto callee_enum = getMemOIREnum(callee_name);

  /*
   * Build the AllocationSummary for the given MemOIR allocation call.
   */
  AllocationSummary *allocation_summary;
  switch (callee_enum) {
    case MemOIR_Func::ALLOCATE_STRUCT:
      allocation_summary = this->getStructAllocationSummary(call_inst);
      break;
    case MemOIR_Func::ALLOCATE_TENSOR:
      allocation_summary = this->getTensorAllocationSummary(call_inst);
      break;
  }

  /*
   * Memoize the AllocationSummary we just built.
   */
  this->allocation_summaries[&call_inst] = allocation_summary;

  /*
   * Return the AllocationSummary.
   */
  return allocation_summary;
}

AllocationSummary *AllocationAnalysis::getStructAllocationSummary(
    CallInst &call_inst) {
  /*
   * Get the Type value from the call.
   */
  auto type_value = call_inst.getArgOperand(0);
  assert(type_value && "in AllocationAnalysis::getStructAllocationSummary"
         && "type argument to call instruction is null");

  /*
   * Determine the type summary.
   */
  auto &type_analysis = TypeAnalysis::get(this->M);
  auto type_summary = type_analysis.getTypeSummary(*type_value);
  assert(type_summary && "in AllocationAnalysis::getStructAllocationSummary"
         && "type summary for struct allocation not found");

  /*
   * Create the allocation.
   */
  return new StructAllocationSummary(type_summary);
}

AllocationSummary *AllocationAnalysis::getTensorAllocationSummary(
    CallInst &call_inst) {
  /*
   * Get the Type value from the call.
   */
  auto type_value = call_inst.getArgOperand(0);
  assert(type_value && "in AllocationAnalysis::getTensorAllocationSummary"
         && "type argument to call instruction is null");

  /*
   * Determine the type summary.
   */
  auto &type_analysis = TypeAnalysis::get(this->M);
  auto element_type_summary = getTypeSummary(*type_value);
  assert(element_type_summary
         && "in AllocationAnalysis::getTensorAllocationSummary"
         && "element type summary for tensor not found");

  /*
   * Get the number of dimensions.
   */
  auto num_dimensions_value = call_inst.getArgOperand(1);
  auto num_dimensions_constant = dyn_cast<ConstantInt>(num_dimensions_value);
  assert(num_dimensions_constant
         && "in AllocationAnalysis::getTensorAllocationSummary"
         && "number of dimensions for tensor allocation is not constant");
  auto num_dimensions = num_dimensions_constant->getZExtValue();

  /*
   * Create the tensor type summary.
   */
  auto type_summary =
      new TensorTypeSummary(element_type_summary, num_dimensions);

  /*
   * Get the length of dimensions.
   */
  std::vector<Value *> length_of_dimensions;
  for (auto i = 0; i < num_dimensions; i++) {
    auto dimension_arg_idx = i + 2;
    auto dimension_length = call_inst.getArgOperand(dimension_arg_idx);
    length_of_dimensions.push_back(dimension_length);
  }

  /*
   * Create the allocation
   */
  return new TensorAllocationSummary(type_summary, length_of_dimensions);
}

TypeSummary *AllocationAnalysis::getTypeSummary(llvm::Value &V) {
  /*
   * Trace back the value to find the associated
   *   TypeSummary, if it exists.
   */

  /*
   * If we have a call instruction,
   *   get its TypeSummary and we are done.
   */
  if (auto call_inst = dyn_cast<CallInst>(&V)) {
    return this->getTypeSummary(*call_inst);
  }

  /*
   * If we have load instruction, trace back to its
   *   global variable and find the original store to it.
   */
  if (auto load_inst = dyn_cast<LoadInst>(&V)) {
    auto load_ptr = load_inst->getPointerOperand();

    auto global = dyn_cast<GlobalVariable>(load_ptr);
    if (!global) {
      if (auto load_gep = dyn_cast<GetElementPtrInst>(load_ptr)) {
        auto gep_ptr = load_gep->getPointerOperand();
        global = dyn_cast<GlobalVariable>(gep_ptr);
      }
    }

    /*
     * If we still cannot find the GlobalVariable, return NULL.
     */
    if (!global) {
      return nullptr;
    }

    /*
     * Find the original store for this global variable.
     */
    for (auto user : global->users()) {
      if (auto store_inst = dyn_cast<StoreInst>(user)) {
        auto store_value = store_inst->getValueOperand();
        auto store_call = dyn_cast<CallInst>(store_value);
        assert(store_call && "in AllocationAnalysis::getTypeSummary"
               && "original store to type's global is not a call");

        return this->getTypeSummary(store_call);
      }

      // TODO: handle GEP's here, hasn't broken yet.
    }
  }

  // TODO: handle PHI and select nodes

  return nullptr;
}

void AllocationAnalysis::invalidate() {
  // TODO
  return;
}

/*
 * Singleton
 */
AllocationAnalysis &AllocationAnalysis::get(Module &M) {
  auto found_analysis = AllocationAnalysis::analyses.find(&M);
  if (found_analysis != AllocationAnalysis::analyses.end()) {
    return *(found_analysis.second);
  }

  auto new_analysis = new AllocationAnalysis(M);
  AllocationAnalysis::analyses[&M] = new_analysis;

  return *new_analysis;
}

void AllocationAnalysis::invalidate(Module &M) {
  auto found_analysis = AllocationAnalysis::analyses.find(&M);
  if (found_analysis != AllocationAnalysis::analyses.end()) {
    auto &analysis = *(found_analysis.second);
    analysis.invalidate();
  }

  return;
}

} // namespace llvm::memoir
