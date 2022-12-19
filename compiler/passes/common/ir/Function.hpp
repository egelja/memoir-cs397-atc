#ifndef COMMON_FUNCTION_H
#define COMMON_FUNCTION_H
#pragma once

#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"

#include "common/ir/Instructions.hpp"
#include "common/ir/Types.hpp"

#include "common/support/InternalDatatypes.hpp"

/*
 * MemOIR wrapper of an LLVM Function.
 *
 * Author(s): Tommy McMichen
 * Created: December 14, 2022
 */

struct MemOIRFunctionType {
public:
  static MemOIRFunctionType &get(llvm::FunctionType &FT,
                                 Type *return_type,
                                 vector<Type *> param_types);

  llvm::FunctionType &getLLVMFunctionType() const;
  Type *getReturnType() const;
  llvm::Type *getReturnLLVMType() const;
  unsigned getNumParams() const;
  Type *getParamType(unsigned param_index) const;
  llvm::Type *getParamLLVMType(unsigned param_index) const;

protected:
  // Owned state

  // Borrowed state
  llvm::FunctionType &FT;
  Type *return_type;
  vector<Type *> param_type;

  MemOIRFunctionType(llvm::FunctionType &FT,
                     Type *return_type,
                     vector<Type *> param_types);
  ~MemOIRFunctionType();

  friend class MemOIRFunction;
};

struct MemOIRInst;

struct MemOIRFunction {
public:
  static MemOIRFunction &get(llvm::Function &F);

  llvm::Module &getParent() const;

  MemOIRFunctionType &getFunctionType() const;
  llvm::Function &getLLVMFunction() const;

  unsigned getNumberOfArguments() const;
  Type *getArgumentType(unsigned arg_index) const;
  llvm::Type *getArgumentLLVMType(unsigned arg_index);
  llvm::Argument &getArgument(unsigned arg_index) const;
  Type *getReturnType() const;
  llvm::Type *getReturnLLVMType() const;

protected:
  // Owned state
  MemOIRFunctionType *function_type;
  vector<MemOIRInst *> memoir_instructions;

  // Borrowed state
  llvm::Function &F;
  map<llvm::Instruction *, MemOIRInst *> llvm_to_memoir_instructions;

  // Global state
  static map<llvm::Function *, MemOIRFunction *> llvm_to_memoir_functions;

  MemOIRFunction(llvm::Function &F);
  ~MemOIRFunction();

  friend class MemOIRModule;
};

#endif
