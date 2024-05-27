#include "packs/merging.hpp"
#include "packs/pack_set.hpp"
#include "packs/seeder.hpp"
#include "packs/extension.hpp"

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

#include "memoir/transforms/vectorization/src/packs/dag.hpp"
#include "memoir/utility/FunctionNames.hpp"
#include "memoir/utility/Metadata.hpp"

/*
 * Author(s): Kevin Hayes
 */

namespace {

using namespace llvm::memoir;
using namespace arcana::noelle;

struct SLPPass : public llvm::ModulePass {
    static char ID;

    SLPPass() : ModulePass(ID) {}

    bool doInitialization(llvm::Module& M) override { return false; }

    bool runOnBasicBlock(llvm::BasicBlock& BB)
    {
        auto& noelle = getAnalysis<Noelle>();
        auto pdg = noelle.getProgramDependenceGraph();
        auto fdg = pdg->createFunctionSubgraph(*BB.getParent());

        PackSeeder visitor(fdg);
        for (llvm::Instruction& i : BB) {
            // llvm::memoir::println(i);
            visitor.visit(i);
        }

        llvm::memoir::println(std::string(80, '-'));

        // find packs
        PackSet seed_packs = visitor.create_seeded_pack_set();
        llvm::memoir::println("Seeded PackSet: ", seed_packs.dbg_string());

        // TODO: Extend the packs with use-def and def-use chains
        PackSet extended_packs = seed_packs;
        PacksetExtender extender(BB, &extended_packs, fdg);

        extender.extend();
        llvm::memoir::println("Extended Packset: ", extended_packs.dbg_string());

        // Combine packs into things that can be vectorized
        auto merged_packs = merge_packs(extended_packs);
        llvm::memoir::println("Merged PackSet: ", merged_packs.dbg_string());

        // create our DAG
        PackDAG dag;

        for (Pack pack : merged_packs)
            dag.add_node(std::move(pack));

        llvm::memoir::println("Graph:", dag.dbg_string());

        return false;
    }

    bool runOnModule(llvm::Module& M) override
    {
        bool changed;

        for (llvm::Function& F : M) {
            for (llvm::BasicBlock& BB : F) {
                changed |= runOnBasicBlock(BB);
            }
        }

        // We did not modify the program, so we return false.
        return changed;
    }

    void getAnalysisUsage(llvm::AnalysisUsage& AU) const override
    {
        AU.addRequired<Noelle>();
        return;
    }
};

// Next there is code to register your pass to "opt"
char SLPPass::ID = 0;
static llvm::RegisterPass<SLPPass>
    X("memoir-vector", "Trying out SLP Vectorization in MemOIR");
} // namespace
