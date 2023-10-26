#ifndef MEMOIR_BUILDER_H
#define MEMOIR_BUILDER_H
#pragma once

#include "llvm/IR/IRBuilder.h"

#include "memoir/ir/Instructions.hpp"
#include "memoir/ir/MutOperations.hpp"
#include "memoir/support/Assert.hpp"
#include "memoir/support/Casting.hpp"
#include "memoir/support/Print.hpp"

#include "memoir/utility/FunctionNames.hpp"

namespace llvm::memoir {

class MemOIRBuilder : public IRBuilder<> {
public:
  /*
   * MemOIRBuilder Constructors
   */
  MemOIRBuilder(llvm::BasicBlock *BB) : M(BB->getModule()), IRBuilder<>(BB) {
    MEMOIR_NULL_CHECK(this->M,
                      "Could not determine LLVM Module of basic block.");
  }

  MemOIRBuilder(llvm::Instruction *IP, bool InsertAfter = true)
    : M(IP->getModule()),
      IRBuilder<>((InsertAfter) ? IP : IP->getNextNode()) {
    MEMOIR_NULL_CHECK(this->M,
                      "Could not determine LLVM Module of insertion point.");
  }

  MemOIRBuilder(MemOIRInst &IP, bool InsertAfter = true)
    : MemOIRBuilder(&IP.getCallInst(), InsertAfter) {}

  /*
   * Type Instructions
   */
  TypeInst *CreateTypeInst(Type &type, const Twine &name = "") {
    if (isa<FloatType>(&type)) {
      return this->CreateFloatTypeInst(name);
    } else if (isa<DoubleType>(&type)) {
      return this->CreateDoubleTypeInst(name);
    } else if (isa<PointerType>(&type)) {
      return this->CreatePointerTypeInst(name);
    } else if (auto *integer_type = dyn_cast<IntegerType>(&type)) {
      if (!integer_type->isSigned()) {
        switch (integer_type->getBitWidth()) {
          case 64:
            return this->CreateUInt64TypeInst(name);
          case 32:
            return this->CreateUInt32TypeInst(name);
          case 16:
            return this->CreateUInt16TypeInst(name);
          case 8:
            return this->CreateUInt8TypeInst(name);
          default:
            MEMOIR_UNREACHABLE(
                "Attempt to create unknown unsigned integer type!");
        }
      } else {
        switch (integer_type->getBitWidth()) {
          case 64:
            return this->CreateInt64TypeInst(name);
          case 32:
            return this->CreateInt32TypeInst(name);
          case 16:
            return this->CreateInt16TypeInst(name);
          case 8:
            return this->CreateInt8TypeInst(name);
          case 2:
            return this->CreateInt2TypeInst(name);
          case 1:
            return this->CreateBoolTypeInst(name);
          default:
            MEMOIR_UNREACHABLE(
                "Attempt to create unknown signed integer type!");
        }
      }
    } else if (auto *ref_type = dyn_cast<ReferenceType>(&type)) {
      return this->CreateReferenceTypeInst(
          &this->CreateTypeInst(ref_type->getReferencedType(), name)
               ->getCallInst(),
          name);
    } else if (auto *struct_type = dyn_cast<StructType>(&type)) {
      auto *name_global = &struct_type->getDefinition().getNameOperand();
      if (auto *name_as_inst = dyn_cast<llvm::Instruction>(name_global)) {
        name_global = name_as_inst->clone();
        this->Insert(cast<llvm::Instruction>(name_global));
      }
      return this->CreateStructTypeInst(name_global, name);
    } else if (auto *field_array_type = dyn_cast<FieldArrayType>(&type)) {
      return this->CreateTypeInst(field_array_type->getStructType());
    } else if (auto *static_tensor_type = dyn_cast<StaticTensorType>(&type)) {
      return nullptr;
    } else if (auto *tensor_type = dyn_cast<TensorType>(&type)) {
      return nullptr;
    } else if (auto *assoc_type = dyn_cast<AssocArrayType>(&type)) {
      return nullptr;
    } else if (auto *seq_type = dyn_cast<SequenceType>(&type)) {
      return nullptr;
    }
    MEMOIR_UNREACHABLE("Attempt to create instruction for unknown type");
  }

  /*
   * Primitive Type Instructions
   */
#define HANDLE_PRIMITIVE_TYPE_INST(ENUM, FUNC, CLASS)                          \
  CLASS *Create##CLASS(const Twine &name = "") {                               \
    auto llvm_func =                                                           \ 
      FunctionNames::get_memoir_function(*(this->M), MemOIR_Func::ENUM);       \
    auto llvm_call = this->CreateCall(llvm_func, None, name);                  \
    MEMOIR_NULL_CHECK(llvm_call,                                               \
                      "Could not construct the LLVM Call to " #FUNC);          \
    auto memoir_inst = MemOIRInst::get(*llvm_call);                            \
    auto type_inst = dyn_cast<CLASS>(memoir_inst);                             \
    return type_inst;                                                          \
  }
#include "memoir/ir/Instructions.def"

  /*
   * Derived Type Instructions
   */
  DefineStructTypeInst *CreateDefineStructTypeInst(
      const char *type_name,
      int num_fields,
      vector<llvm::Value *> field_types,
      const Twine &name = "") {
    // Fetch the LLVM Function.
    auto llvm_func =
        FunctionNames::get_memoir_function(*(this->M),
                                           MemOIR_Func::DEFINE_STRUCT_TYPE);

    // Create the LLVM type name and number of fields constant.
    auto llvm_type_name = this->CreateGlobalString(type_name, "type.struct.");
    auto llvm_num_fields = this->getInt64(num_fields);

    // Build the list of arguments.
    auto llvm_args = vector<llvm::Value *>({ llvm_type_name, llvm_num_fields });
    for (auto field_type : field_types) {
      llvm_args.push_back(field_type);
    }

    // Create the call.
    auto llvm_call = this->CreateCall(FunctionCallee(llvm_func),
                                      llvm::ArrayRef(llvm_args),
                                      name);

    // Convert to MemOIRInst and return.
    auto memoir_inst = MemOIRInst::get(*llvm_call);
    auto define_struct_type_inst = dyn_cast<DefineStructTypeInst>(memoir_inst);
    return define_struct_type_inst;
  }

  StructTypeInst *CreateStructTypeInst(llvm::Value *llvm_type_name,
                                       const Twine &name = "") {
    // Fetch the LLVM Function.
    auto llvm_func =
        FunctionNames::get_memoir_function(*(this->M),
                                           MemOIR_Func::STRUCT_TYPE);

    // Create the call.
    auto llvm_args = vector<llvm::Value *>({ llvm_type_name });
    auto llvm_call = this->CreateCall(FunctionCallee(llvm_func),
                                      llvm::ArrayRef(llvm_args),
                                      name);

    // Convert to MemOIRInst and return.
    auto memoir_inst = MemOIRInst::get(*llvm_call);
    auto struct_type_inst = dyn_cast<StructTypeInst>(memoir_inst);
    return struct_type_inst;
  }

  ReferenceTypeInst *CreateReferenceTypeInst(llvm::Value *referenced_type,
                                             const Twine &name = "") {
    // Fetch the LLVM Function.
    auto llvm_func =
        FunctionNames::get_memoir_function(*(this->M),
                                           MemOIR_Func::REFERENCE_TYPE);

    // Build the list of arguments.
    auto llvm_args = vector<llvm::Value *>({ referenced_type });

    // Create the call.
    auto llvm_call = this->CreateCall(FunctionCallee(llvm_func),
                                      llvm::ArrayRef(llvm_args),
                                      name);

    // Convert to MemOIRInst and return.
    auto memoir_inst = MemOIRInst::get(*llvm_call);
    auto ref_type_inst = dyn_cast<ReferenceTypeInst>(memoir_inst);
    return ref_type_inst;
  }

  // TODO: Add the other derived type instructions.

  /*
   * Allocation Instructions
   */
  SequenceAllocInst *CreateSequenceAllocInst(Type &type,
                                             uint64_t size,
                                             const Twine &name = "") {
    return this->CreateSequenceAllocInst(
        &this->CreateTypeInst(type)->getCallInst(),
        size,
        name);
  }

  SequenceAllocInst *CreateSequenceAllocInst(Type &type,
                                             llvm::Value *size,
                                             const Twine &name = "") {
    return this->CreateSequenceAllocInst(
        &this->CreateTypeInst(type)->getCallInst(),
        size,
        name);
  }

  SequenceAllocInst *CreateSequenceAllocInst(llvm::Value *type,
                                             uint64_t size,
                                             const Twine &name = "") {
    return this->CreateSequenceAllocInst(type, this->getInt64(size), name);
  }

  SequenceAllocInst *CreateSequenceAllocInst(llvm::Value *type,
                                             llvm::Value *size,
                                             const Twine &name = "") {
    // Fetch the LLVM Function.
    auto llvm_func =
        FunctionNames::get_memoir_function(*(this->M),
                                           MemOIR_Func::ALLOCATE_SEQUENCE);

    // Create the LLVM call.
    auto llvm_call = this->CreateCall(FunctionCallee(llvm_func),
                                      llvm::ArrayRef({ type, size }),
                                      name);
    MEMOIR_NULL_CHECK(llvm_call,
                      "Could not create the call for sequence allocation.");

    // Cast to MemOIRInst and return.
    auto memoir_inst = MemOIRInst::get(*llvm_call);
    auto seq_alloc_inst = dyn_cast<SequenceAllocInst>(memoir_inst);
    MEMOIR_NULL_CHECK(seq_alloc_inst,
                      "Could not create call to AllocateSequence");
    return seq_alloc_inst;
  }

  // Assoc allocation
  AssocArrayAllocInst *CreateAssocArrayAllocInst(Type &key_type,
                                                 Type &value_type,
                                                 const Twine &name = "") {
    return this->CreateAssocArrayAllocInst(
        &this->CreateTypeInst(key_type)->getCallInst(),
        &this->CreateTypeInst(value_type)->getCallInst(),
        name);
  }

  AssocArrayAllocInst *CreateAssocArrayAllocInst(Type &key_type,
                                                 llvm::Value *value_type,
                                                 const Twine &name = "") {
    return this->CreateAssocArrayAllocInst(
        &this->CreateTypeInst(key_type)->getCallInst(),
        value_type,
        name);
  }

  AssocArrayAllocInst *CreateAssocArrayAllocInst(llvm::Value *key_type,
                                                 llvm::Value *value_type,
                                                 const Twine &name = "") {
    // Fetch the LLVM Function.
    auto *llvm_func =
        FunctionNames::get_memoir_function(*(this->M),
                                           MemOIR_Func::ALLOCATE_ASSOC_ARRAY);

    // Create the LLVM call.
    auto *llvm_call = this->CreateCall(FunctionCallee(llvm_func),
                                       llvm::ArrayRef({ key_type, value_type }),
                                       name);
    MEMOIR_NULL_CHECK(llvm_call,
                      "Could not create the call for sequence allocation.");

    // Cast to MemOIRInst and return.
    auto *memoir_inst = MemOIRInst::get(*llvm_call);
    auto *alloc_inst = dyn_cast<AssocArrayAllocInst>(memoir_inst);
    MEMOIR_NULL_CHECK(alloc_inst, "Could not create AssocArrayAllocInst");

    return alloc_inst;
  }

  // Access Instructions
  IndexWriteInst *CreateIndexWriteInst(Type &element_type,
                                       llvm::Value *llvm_value_to_write,
                                       llvm::Value *llvm_collection,
                                       llvm::Value *llvm_index,
                                       const Twine &name = "") {
    // Fetch the LLVM Function.
    auto llvm_func = FunctionNames::get_memoir_function(
        *(this->M),
        getIndexWriteEnumForType(element_type));

    // Create the LLVM call.
    auto llvm_call = this->CreateCall(
        FunctionCallee(llvm_func),
        llvm::ArrayRef({ llvm_value_to_write, llvm_collection, llvm_index }),
        name);
    MEMOIR_NULL_CHECK(llvm_call,
                      "Could not create the call for index write operation.");

    // Cast to MemOIRInst and return.
    auto memoir_inst = MemOIRInst::get(*llvm_call);
    auto inst = dyn_cast<IndexWriteInst>(memoir_inst);
    MEMOIR_NULL_CHECK(inst, "Could not create call to IndexWriteInst");
    return inst;
  }

  MutIndexWriteInst *CreateMutIndexWriteInst(Type &element_type,
                                             llvm::Value *llvm_value_to_write,
                                             llvm::Value *llvm_collection,
                                             llvm::Value *llvm_index,
                                             const Twine &name = "") {
    // Fetch the LLVM Function.
    auto llvm_func = FunctionNames::get_memoir_function(
        *(this->M),
        getMutIndexWriteEnumForType(element_type));

    // Create the LLVM call.
    auto llvm_call = this->CreateCall(
        FunctionCallee(llvm_func),
        llvm::ArrayRef({ llvm_value_to_write, llvm_collection, llvm_index }),
        name);
    MEMOIR_NULL_CHECK(llvm_call,
                      "Could not create the call for index write operation.");

    // Cast to MemOIRInst and return.
    auto memoir_inst = MemOIRInst::get(*llvm_call);
    auto inst = dyn_cast<MutIndexWriteInst>(memoir_inst);
    MEMOIR_NULL_CHECK(inst, "Could not create call to MutIndexWriteInst");
    return inst;
  }

  AssocWriteInst *CreateAssocWriteInst(Type &element_type,
                                       llvm::Value *llvm_value_to_write,
                                       llvm::Value *llvm_collection,
                                       llvm::Value *llvm_assoc,
                                       const Twine &name = "") {
    // Fetch the LLVM Function.
    auto llvm_func = FunctionNames::get_memoir_function(
        *(this->M),
        getAssocWriteEnumForType(element_type));

    // Create the LLVM call.
    auto llvm_call = this->CreateCall(
        FunctionCallee(llvm_func),
        llvm::ArrayRef({ llvm_value_to_write, llvm_collection, llvm_assoc }),
        name);
    MEMOIR_NULL_CHECK(llvm_call,
                      "Could not create the call for assoc write operation.");

    // Cast to MemOIRInst and return.
    auto memoir_inst = MemOIRInst::get(*llvm_call);
    auto inst = dyn_cast<AssocWriteInst>(memoir_inst);
    MEMOIR_NULL_CHECK(inst, "Could not create call to AssocWriteInst");
    return inst;
  }

  MutAssocWriteInst *CreateMutAssocWriteInst(Type &element_type,
                                             llvm::Value *llvm_value_to_write,
                                             llvm::Value *llvm_collection,
                                             llvm::Value *llvm_assoc,
                                             const Twine &name = "") {
    // Fetch the LLVM Function.
    auto llvm_func = FunctionNames::get_memoir_function(
        *(this->M),
        getMutAssocWriteEnumForType(element_type));

    // Create the LLVM call.
    auto llvm_call = this->CreateCall(
        FunctionCallee(llvm_func),
        llvm::ArrayRef({ llvm_value_to_write, llvm_collection, llvm_assoc }),
        name);
    MEMOIR_NULL_CHECK(llvm_call,
                      "Could not create the call for assoc write operation.");

    // Cast to MemOIRInst and return.
    auto memoir_inst = MemOIRInst::get(*llvm_call);
    auto inst = dyn_cast<MutAssocWriteInst>(memoir_inst);
    MEMOIR_NULL_CHECK(inst, "Could not create call to MutAssocWriteInst");
    return inst;
  }

  // Deletion Instructions
  DeleteStructInst *CreateDeleteStructInst(llvm::Value *struct_to_delete) {
    // Fetch the LLVM Function.
    auto llvm_func =
        FunctionNames::get_memoir_function(*(this->M),
                                           MemOIR_Func::DELETE_STRUCT);

    // Create the LLVM call.
    auto llvm_call = this->CreateCall(FunctionCallee(llvm_func),
                                      llvm::ArrayRef({ struct_to_delete }));
    MEMOIR_NULL_CHECK(llvm_call,
                      "Could not create the call for delete struct operation.");

    // Cast to MemOIRInst and return.
    auto memoir_inst = MemOIRInst::get(*llvm_call);
    auto delete_inst = dyn_cast_or_null<DeleteStructInst>(memoir_inst);
    MEMOIR_NULL_CHECK(delete_inst,
                      "Could not create call to DeleteCollectionInst");

    return delete_inst;
  }

  DeleteCollectionInst *CreateDeleteCollectionInst(
      llvm::Value *collection_to_delete) {
    // Fetch the LLVM Function.
    auto llvm_func =
        FunctionNames::get_memoir_function(*(this->M),
                                           MemOIR_Func::DELETE_COLLECTION);

    // Create the LLVM call.
    auto llvm_call = this->CreateCall(FunctionCallee(llvm_func),
                                      llvm::ArrayRef({ collection_to_delete }));
    MEMOIR_NULL_CHECK(
        llvm_call,
        "Could not create the call for delete collection operation.");

    // Cast to MemOIRInst and return.
    auto memoir_inst = MemOIRInst::get(*llvm_call);
    auto delete_inst = dyn_cast_or_null<DeleteCollectionInst>(memoir_inst);
    MEMOIR_NULL_CHECK(delete_inst,
                      "Could not create call to DeleteCollectionInst");

    return delete_inst;
  }

  // Sequence instructions.
  SeqCopyInst *CreateSeqCopyInst(llvm::Value *collection,
                                 llvm::Value *left,
                                 llvm::Value *right,
                                 const Twine &name = "") {
    // Fetch the LLVM Function.
    auto llvm_func =
        FunctionNames::get_memoir_function(*(this->M), MemOIR_Func::SEQ_COPY);

    // Create the LLVM call.
    auto llvm_call =
        this->CreateCall(FunctionCallee(llvm_func),
                         llvm::ArrayRef({ collection, left, right }),
                         name);
    MEMOIR_NULL_CHECK(llvm_call,
                      "Could not create the call for slice operation.");

    // Cast to MemOIRInst and return.
    auto memoir_inst = MemOIRInst::get(*llvm_call);
    auto inst = dyn_cast<SeqCopyInst>(memoir_inst);
    MEMOIR_NULL_CHECK(inst, "Could not create call to SeqCopyInst");
    return inst;
  }

  SeqCopyInst *CreateSeqCopyInst(llvm::Value *collection_to_slice,
                                 llvm::Value *left,
                                 int64_t right,
                                 const Twine &name = "") {
    // Create the right constant.
    auto right_constant = this->getInt64(right);

    // Call the base builder method.
    return this->CreateSeqCopyInst(collection_to_slice,
                                   left,
                                   right_constant,
                                   name);
  }

  SeqCopyInst *CreateSeqCopyInst(llvm::Value *collection_to_slice,
                                 int64_t left,
                                 llvm::Value *right,
                                 const Twine &name = "") {
    // Create the left constant.
    auto left_constant = this->getInt64(left);

    // Call the base builder method.
    return this->CreateSeqCopyInst(collection_to_slice,
                                   left_constant,
                                   right,
                                   name);
  }

  SeqCopyInst *CreateSeqCopyInst(llvm::Value *collection_to_slice,
                                 int64_t left,
                                 int64_t right,
                                 const Twine &name = "") {
    // Create the left constant.
    auto left_constant = this->getInt64(left);

    // Create the right constant.
    auto right_constant = this->getInt64(right);

    // Call the base builder method.
    return this->CreateSeqCopyInst(collection_to_slice,
                                   left_constant,
                                   right_constant,
                                   name);
  }

  // SSA Assoc operations.
  AssocInsertInst *CreateAssocInsertInst(llvm::Value *collection,
                                         llvm::Value *key_value,
                                         const Twine &name = "") {
    // Fetch the LLVM Function.
    auto llvm_func =
        FunctionNames::get_memoir_function(*(this->M),
                                           MemOIR_Func::ASSOC_INSERT);

    // Create the LLVM call.
    auto llvm_call = this->CreateCall(FunctionCallee(llvm_func),
                                      llvm::ArrayRef({ collection, key_value }),
                                      name);
    MEMOIR_NULL_CHECK(llvm_call,
                      "Could not create the call for assoc insert operation.");

    // Cast to MemOIRInst and return.
    auto memoir_inst = MemOIRInst::get(*llvm_call);
    auto inst = dyn_cast<AssocInsertInst>(memoir_inst);
    MEMOIR_NULL_CHECK(inst, "Could not create call to AssocInsertInst");
    return inst;
  }

  AssocRemoveInst *CreateAssocRemoveInst(llvm::Value *collection,
                                         llvm::Value *key_value,
                                         const Twine &name = "") {
    // Fetch the LLVM Function.
    auto llvm_func =
        FunctionNames::get_memoir_function(*(this->M),
                                           MemOIR_Func::ASSOC_REMOVE);

    // Create the LLVM call.
    auto llvm_call = this->CreateCall(FunctionCallee(llvm_func),
                                      llvm::ArrayRef({ collection, key_value }),
                                      name);
    MEMOIR_NULL_CHECK(llvm_call,
                      "Could not create the call for AssocRemoveInst.");

    // Cast to MemOIRInst and return.
    auto memoir_inst = MemOIRInst::get(*llvm_call);
    auto inst = dyn_cast<AssocRemoveInst>(memoir_inst);
    MEMOIR_NULL_CHECK(inst, "Type mismatch for AssocRemoveInst");
    return inst;
  }

  // General-purpose collection operations.
  SizeInst *CreateSizeInst(llvm::Value *collection_to_slice,
                           const Twine &name = "") {
    // Fetch the LLVM Function.
    auto llvm_func =
        FunctionNames::get_memoir_function(*(this->M), MemOIR_Func::SIZE);

    // Create the LLVM call.
    auto llvm_call = this->CreateCall(FunctionCallee(llvm_func),
                                      llvm::ArrayRef({ collection_to_slice }),
                                      name);
    MEMOIR_NULL_CHECK(llvm_call,
                      "Could not create the call for size operation.");

    // Cast to MemOIRInst and return.
    auto memoir_inst = MemOIRInst::get(*llvm_call);
    auto inst = dyn_cast<SizeInst>(memoir_inst);
    MEMOIR_NULL_CHECK(inst, "Could not create call to SizeInst");
    return inst;
  }

  EndInst *CreateEndInst(const Twine &name = "") {
    // Fetch the LLVM Function.
    auto llvm_func =
        FunctionNames::get_memoir_function(*(this->M), MemOIR_Func::END);

    // Create the LLVM call.
    auto llvm_call = this->CreateCall(FunctionCallee(llvm_func),
                                      llvm::ArrayRef<llvm::Value *>({}),
                                      name);
    MEMOIR_NULL_CHECK(llvm_call,
                      "Could not create the call for end operation.");

    // Cast to MemOIRInst and return.
    auto memoir_inst = MemOIRInst::get(*llvm_call);
    auto inst = dyn_cast<EndInst>(memoir_inst);
    MEMOIR_NULL_CHECK(inst, "Could not create call to EndInst");
    return inst;
  }

  /*
   * SSA/readonc operations.
   */
  UsePHIInst *CreateUsePHI(llvm::Value *collection, const Twine &name = "") {
    // Fetch the LLVM Function.
    auto llvm_func =
        FunctionNames::get_memoir_function(*(this->M), MemOIR_Func::USE_PHI);

    // Create the LLVM call.
    auto llvm_call = this->CreateCall(FunctionCallee(llvm_func),
                                      llvm::ArrayRef({ collection }),
                                      name);
    MEMOIR_NULL_CHECK(llvm_call,
                      "Could not create the call for UsePHI operation.");

    // Cast to MemOIRInst and return.
    auto memoir_inst = MemOIRInst::get(*llvm_call);
    auto inst = dyn_cast<UsePHIInst>(memoir_inst);
    MEMOIR_NULL_CHECK(inst, "Could not create call to UsePHIInst");
    return inst;
  }

  DefPHIInst *CreateDefPHI(llvm::Value *collection, const Twine &name = "") {
    // Fetch the LLVM Function.
    auto llvm_func =
        FunctionNames::get_memoir_function(*(this->M), MemOIR_Func::DEF_PHI);

    // Create the LLVM call.
    auto llvm_call = this->CreateCall(FunctionCallee(llvm_func),
                                      llvm::ArrayRef({ collection }),
                                      name);
    MEMOIR_NULL_CHECK(llvm_call,
                      "Could not create the call for DefPHI operation.");

    // Cast to MemOIRInst and return.
    auto memoir_inst = MemOIRInst::get(*llvm_call);
    auto inst = dyn_cast<DefPHIInst>(memoir_inst);
    MEMOIR_NULL_CHECK(inst, "Could not create call to DefPHIInst");
    return inst;
  }

  /*
   * Mutable sequence operations.
   */
  MutSeqAppendInst *CreateMutSeqAppendInst(llvm::Value *collection,
                                           llvm::Value *collection_to_append,
                                           const Twine &name = "") {
    // Fetch the LLVM Function.
    auto llvm_func =
        FunctionNames::get_memoir_function(*(this->M),
                                           MemOIR_Func::MUT_SEQ_APPEND);
    MEMOIR_NULL_CHECK(llvm_func, "Couldn't find the mut function!");

    // Create the LLVM call.
    auto llvm_call =
        this->CreateCall(FunctionCallee(llvm_func),
                         llvm::ArrayRef({ collection, collection_to_append }),
                         name);
    MEMOIR_NULL_CHECK(
        llvm_call,
        "Could not create the call for sequence append operation.");

    // Cast to MemOIRInst and return.
    auto memoir_inst = MemOIRInst::get(*llvm_call);
    auto inst = dyn_cast<MutSeqAppendInst>(memoir_inst);
    MEMOIR_NULL_CHECK(inst, "Could not create call to SeqAppendInst");
    return inst;
  }

  SeqSwapInst *CreateSeqSwapInst(llvm::Value *from_collection,
                                 llvm::Value *from_begin,
                                 llvm::Value *from_end,
                                 llvm::Value *to_collection,
                                 llvm::Value *to_begin,
                                 const Twine &name = "") {
    // Fetch the LLVM Function.
    auto *llvm_func =
        FunctionNames::get_memoir_function(*(this->M), MemOIR_Func::SEQ_SWAP);
    MEMOIR_NULL_CHECK(llvm_func, "Couldn't find the memoir function!");

    // Create the LLVM call.
    auto *llvm_call = this->CreateCall(
        FunctionCallee(llvm_func),
        llvm::ArrayRef(
            { from_collection, from_begin, from_end, to_collection, to_begin }),
        name);
    MEMOIR_NULL_CHECK(llvm_call,
                      "Could not create the call for sequence swap operation.");

    // Cast to MemOIRInst and return.
    auto *memoir_inst = MemOIRInst::get(*llvm_call);
    auto *inst = dyn_cast_or_null<SeqSwapInst>(memoir_inst);
    MEMOIR_NULL_CHECK(inst, "Could not create call to SeqSwapInst");
    return inst;
  }

  SeqSwapWithinInst *CreateSeqSwapWithinInst(llvm::Value *collection,
                                             llvm::Value *from_begin,
                                             llvm::Value *from_end,
                                             llvm::Value *to_begin,
                                             const Twine &name = "") {
    // Fetch the LLVM Function.
    auto *llvm_func =
        FunctionNames::get_memoir_function(*(this->M),
                                           MemOIR_Func::SEQ_SWAP_WITHIN);
    MEMOIR_NULL_CHECK(llvm_func, "Couldn't find the memoir function!");

    // Create the LLVM call.
    auto *llvm_call = this->CreateCall(
        FunctionCallee(llvm_func),
        llvm::ArrayRef(
            { collection, from_begin, from_end, collection, to_begin }),
        name);
    MEMOIR_NULL_CHECK(llvm_call,
                      "Could not create the call for sequence swap operation.");

    // Cast to MemOIRInst and return.
    auto *memoir_inst = MemOIRInst::get(*llvm_call);
    auto *inst = dyn_cast_or_null<SeqSwapWithinInst>(memoir_inst);
    MEMOIR_NULL_CHECK(inst, "Could not create call to SeqSwapWithinInst");
    return inst;
  }

  MutSeqSwapInst *CreateMutSeqSwapInst(llvm::Value *from_collection,
                                       llvm::Value *from_begin,
                                       llvm::Value *from_end,
                                       llvm::Value *to_collection,
                                       llvm::Value *to_begin,
                                       const Twine &name = "") {
    // Fetch the LLVM Function.
    auto *llvm_func =
        FunctionNames::get_memoir_function(*(this->M),
                                           MemOIR_Func::MUT_SEQ_SWAP_WITHIN);
    MEMOIR_NULL_CHECK(llvm_func, "Couldn't find the memoir function!");

    // Create the LLVM call.
    auto *llvm_call = this->CreateCall(
        FunctionCallee(llvm_func),
        llvm::ArrayRef(
            { from_collection, from_begin, from_end, to_collection, to_begin }),
        name);
    MEMOIR_NULL_CHECK(llvm_call,
                      "Could not create the call for sequence swap operation.");

    // Cast to MemOIRInst and return.
    auto *memoir_inst = MemOIRInst::get(*llvm_call);
    auto *inst = dyn_cast_or_null<MutSeqSwapInst>(memoir_inst);
    MEMOIR_NULL_CHECK(inst, "Could not create call to SeqSwapInst");
    return inst;
  }

  MutSeqSwapWithinInst *CreateMutSeqSwapWithinInst(llvm::Value *collection,
                                                   llvm::Value *from_begin,
                                                   llvm::Value *from_end,
                                                   llvm::Value *to_begin,
                                                   const Twine &name = "") {
    // Fetch the LLVM Function.
    auto *llvm_func =
        FunctionNames::get_memoir_function(*(this->M),
                                           MemOIR_Func::MUT_SEQ_SWAP_WITHIN);
    MEMOIR_NULL_CHECK(llvm_func, "Couldn't find the memoir function!");

    // Create the LLVM call.
    auto *llvm_call = this->CreateCall(
        FunctionCallee(llvm_func),
        llvm::ArrayRef(
            { collection, from_begin, from_end, collection, to_begin }),
        name);
    MEMOIR_NULL_CHECK(llvm_call,
                      "Could not create the call for sequence swap operation.");

    // Cast to MemOIRInst and return.
    auto *memoir_inst = MemOIRInst::get(*llvm_call);
    auto *inst = dyn_cast_or_null<MutSeqSwapWithinInst>(memoir_inst);
    MEMOIR_NULL_CHECK(inst, "Could not create call to SeqSwapWithinInst");
    return inst;
  }

  SeqRemoveInst *CreateSeqRemoveInst(llvm::Value *collection,
                                     llvm::Value *begin,
                                     llvm::Value *end,
                                     const Twine &name = "") {
    // Fetch the LLVM Function.
    auto *llvm_func =
        FunctionNames::get_memoir_function(*(this->M), MemOIR_Func::SEQ_REMOVE);
    MEMOIR_NULL_CHECK(llvm_func, "Couldn't find the memoir function!");

    // Create the LLVM call.
    auto *llvm_call =
        this->CreateCall(FunctionCallee(llvm_func),
                         llvm::ArrayRef({ collection, begin, end }),
                         name);
    MEMOIR_NULL_CHECK(
        llvm_call,
        "Could not create the call for sequence remove operation.");

    // Cast to MemOIRInst and return.
    auto *memoir_inst = MemOIRInst::get(*llvm_call);
    auto *inst = dyn_cast_or_null<SeqRemoveInst>(memoir_inst);
    MEMOIR_NULL_CHECK(inst, "Could not create call to SeqRemoveInst");
    return inst;
  }

  MutSeqRemoveInst *CreateMutSeqRemoveInst(llvm::Value *collection,
                                           llvm::Value *begin,
                                           llvm::Value *end,
                                           const Twine &name = "") {
    // Fetch the LLVM Function.
    auto *llvm_func =
        FunctionNames::get_memoir_function(*(this->M),
                                           MemOIR_Func::MUT_SEQ_REMOVE);
    MEMOIR_NULL_CHECK(llvm_func, "Couldn't find the memoir function!");

    // Create the LLVM call.
    auto *llvm_call =
        this->CreateCall(FunctionCallee(llvm_func),
                         llvm::ArrayRef({ collection, begin, end }),
                         name);
    MEMOIR_NULL_CHECK(
        llvm_call,
        "Could not create the call for sequence remove operation.");

    // Cast to MemOIRInst and return.
    auto *memoir_inst = MemOIRInst::get(*llvm_call);
    auto *inst = dyn_cast_or_null<MutSeqRemoveInst>(memoir_inst);
    MEMOIR_NULL_CHECK(inst, "Could not create call to SeqRemoveInst");
    return inst;
  }

  SeqInsertInst *CreateSeqInsertInst(Type &element_type,
                                     llvm::Value *llvm_value_to_write,
                                     llvm::Value *llvm_collection,
                                     llvm::Value *llvm_index,
                                     const Twine &name = "") {
    // Fetch the LLVM Function.
    println(element_type);
    auto *llvm_func = FunctionNames::get_memoir_function(
        *(this->M),
        getSeqInsertEnumForType(element_type));

    // Create the LLVM call.
    auto *llvm_call = this->CreateCall(
        FunctionCallee(llvm_func),
        llvm::ArrayRef({ llvm_value_to_write, llvm_collection, llvm_index }),
        name);
    MEMOIR_NULL_CHECK(llvm_call, "Could not create the call to SeqInsertInst.");

    // Cast to MemOIRInst and return.
    auto *memoir_inst = MemOIRInst::get(*llvm_call);
    auto *inst = dyn_cast<SeqInsertInst>(memoir_inst);
    MEMOIR_NULL_CHECK(inst, "Mismatched type for SeqInsertInst creation.");
    return inst;
  }

  SeqInsertSeqInst *CreateSeqInsertSeqInst(llvm::Value *collection_to_insert,
                                           llvm::Value *collection,
                                           llvm::Value *insertion_point,
                                           const Twine &name = "") {
    // Fetch the LLVM Function.
    auto llvm_func =
        FunctionNames::get_memoir_function(*(this->M), MemOIR_Func::SEQ_INSERT);
    MEMOIR_NULL_CHECK(llvm_func, "Couldn't find the memoir function!");

    // Create the LLVM call.
    auto llvm_call = this->CreateCall(
        FunctionCallee(llvm_func),
        llvm::ArrayRef({ collection_to_insert, collection, insertion_point }),
        name);
    MEMOIR_NULL_CHECK(
        llvm_call,
        "Could not create the call for sequence insert operation.");

    // Cast to MemOIRInst and return.
    auto memoir_inst = MemOIRInst::get(*llvm_call);
    auto inst = dyn_cast<SeqInsertSeqInst>(memoir_inst);
    MEMOIR_NULL_CHECK(inst, "Could not create call to SeqInsertSeqInst");
    return inst;
  }

  MutSeqInsertSeqInst *CreateMutCreateSeqInsertSeqInst(
      llvm::Value *collection_to_insert,
      llvm::Value *collection,
      llvm::Value *insertion_point,
      const Twine &name = "") {
    // Fetch the LLVM Function.
    auto llvm_func =
        FunctionNames::get_memoir_function(*(this->M),
                                           MemOIR_Func::MUT_SEQ_INSERT);
    MEMOIR_NULL_CHECK(llvm_func, "Couldn't find the memoir function!");

    // Create the LLVM call.
    auto llvm_call = this->CreateCall(
        FunctionCallee(llvm_func),
        llvm::ArrayRef({ collection_to_insert, collection, insertion_point }),
        name);
    MEMOIR_NULL_CHECK(
        llvm_call,
        "Could not create the call for sequence insert operation.");

    // Cast to MemOIRInst and return.
    auto memoir_inst = MemOIRInst::get(*llvm_call);
    auto inst = dyn_cast<MutSeqInsertSeqInst>(memoir_inst);
    MEMOIR_NULL_CHECK(inst, "Could not create call to MutSeqInsertSeqInst");
    return inst;
  }

protected:
  // Borrowed state
  llvm::Module *M;

// Helper Functions
#define ENUM_FOR_PRIMITIVE_TYPE(ENUM_PREFIX, NAME)                             \
  MemOIR_Func get##NAME##EnumForType(Type &type) {                             \
    if (isa<FloatType>(&type)) {                                               \
      return MemOIR_Func::ENUM_PREFIX##_FLOAT;                                 \
    } else if (isa<DoubleType>(&type)) {                                       \
      return MemOIR_Func::ENUM_PREFIX##_DOUBLE;                                \
    } else if (isa<PointerType>(&type)) {                                      \
      return MemOIR_Func::ENUM_PREFIX##_PTR;                                   \
    } else if (auto *integer_type = dyn_cast<IntegerType>(&type)) {            \
      if (integer_type->isSigned()) {                                          \
        switch (integer_type->getBitWidth()) {                                 \
          case 64:                                                             \
            return MemOIR_Func::ENUM_PREFIX##_UINT64;                          \
          case 32:                                                             \
            return MemOIR_Func::ENUM_PREFIX##_UINT32;                          \
          case 16:                                                             \
            return MemOIR_Func::ENUM_PREFIX##_UINT16;                          \
          case 8:                                                              \
            return MemOIR_Func::ENUM_PREFIX##_UINT8;                           \
          default:                                                             \
            MEMOIR_UNREACHABLE(                                                \
                "Attempt to create unknown unsigned integer type!");           \
        }                                                                      \
      } else {                                                                 \
        switch (integer_type->getBitWidth()) {                                 \
          case 64:                                                             \
            return MemOIR_Func::ENUM_PREFIX##_INT64;                           \
          case 32:                                                             \
            return MemOIR_Func::ENUM_PREFIX##_INT32;                           \
          case 16:                                                             \
            return MemOIR_Func::ENUM_PREFIX##_INT16;                           \
          case 8:                                                              \
            return MemOIR_Func::ENUM_PREFIX##_INT8;                            \
          case 2:                                                              \
            return MemOIR_Func::ENUM_PREFIX##_INT2;                            \
          case 1:                                                              \
            return MemOIR_Func::ENUM_PREFIX##_BOOL;                            \
          default:                                                             \
            MEMOIR_UNREACHABLE(                                                \
                "Attempt to create unknown signed integer type!");             \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    MEMOIR_UNREACHABLE("Attempt to create instruction for unknown type");      \
  };

  ENUM_FOR_PRIMITIVE_TYPE(INDEX_READ, IndexRead)
  ENUM_FOR_PRIMITIVE_TYPE(ASSOC_READ, AssocRead)
  ENUM_FOR_PRIMITIVE_TYPE(STRUCT_READ, StructRead)
  ENUM_FOR_PRIMITIVE_TYPE(INDEX_WRITE, IndexWrite)
  ENUM_FOR_PRIMITIVE_TYPE(ASSOC_WRITE, AssocWrite)
  ENUM_FOR_PRIMITIVE_TYPE(STRUCT_WRITE, StructWrite)
  ENUM_FOR_PRIMITIVE_TYPE(SEQ_INSERT, SeqInsert)
  ENUM_FOR_PRIMITIVE_TYPE(MUT_INDEX_WRITE, MutIndexWrite)
  ENUM_FOR_PRIMITIVE_TYPE(MUT_ASSOC_WRITE, MutAssocWrite)
  ENUM_FOR_PRIMITIVE_TYPE(MUT_STRUCT_WRITE, MutStructWrite)

#define ENUM_FOR_NESTED_TYPE(ENUM_PREFIX, NAME)                                \
  MemOIR_Func get##NAME##EnumForType(Type &type) {                             \
    if (isa<StructType>(&type)) {                                              \
      return MemOIR_Func::ENUM_PREFIX##_STRUCT;                                \
    } else if (isa<CollectionType>(&type)) {                                   \
      return MemOIR_Func::ENUM_PREFIX##_COLLECTION;                            \
    }                                                                          \
    MEMOIR_UNREACHABLE("Attempt to create instruction for unknown type");      \
  };

  ENUM_FOR_NESTED_TYPE(INDEX_GET, IndexGet)
  ENUM_FOR_NESTED_TYPE(ASSOC_GET, AssocGet)
  ENUM_FOR_NESTED_TYPE(STRUCT_GET, StructGet)

}; // namespace llvm::memoir

} // namespace llvm::memoir

#endif
