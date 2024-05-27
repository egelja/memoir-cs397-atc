#pragma once

#include "pack.hpp"
#include "pack_set.hpp"

#include <llvm/IR/Function.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>
#include <llvm/Pass.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

#include "memoir/analysis/TypeAnalysis.hpp"
#include "memoir/ir/Instructions.hpp"
#include "memoir/support/InternalDatatypes.hpp"
#include "memoir/support/Print.hpp"
#include "noelle/core/Noelle.hpp"

#include "memoir/utility/FunctionNames.hpp"
#include "memoir/utility/Metadata.hpp"

using namespace llvm::memoir;
using namespace arcana::noelle;

class PackSeeder : public llvm::memoir::InstVisitor<PackSeeder, void> {
    // In order for the wrapper to work, we need to declare our parent classes as
    // friends.
    friend class llvm::memoir::InstVisitor<PackSeeder, void>;
    friend class llvm::InstVisitor<PackSeeder, void>;

    std::map<llvm::memoir::MemOIR_Func, std::set<MemOIRInst*>> right_free_;
    std::map<llvm::memoir::MemOIR_Func, std::set<MemOIRInst*>> left_free_;

    Noelle* noelle;
public:
    PackSeeder() { return; }

    PackSeeder(Noelle* n);

    void visitInstruction(llvm::Instruction& I)
    {
        // Do nothing.
        return;
    }

    void visitIndexReadInst(IndexReadInst& I);

    PackSet create_seeded_pack_set();

private:
    bool indices_adjacent_(llvm::Value& left, llvm::Value& right);

    void process_index_read_seeds_(PackSet& ps);
};
