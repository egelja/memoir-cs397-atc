#include "packs/pack_set.hpp"

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
#include "memoir/transforms/vectorization/src/packs/merging.hpp"
#include "memoir/utility/FunctionNames.hpp"
#include "memoir/utility/Metadata.hpp"

/*
 * Author(s): Kevin Hayes
 */

namespace {

using namespace llvm::memoir;

class PackSeeder : public llvm::memoir::InstVisitor<PackSeeder, void> {
    // In order for the wrapper to work, we need to declare our parent classes as
    // friends.
    friend class llvm::memoir::InstVisitor<PackSeeder, void>;
    friend class llvm::InstVisitor<PackSeeder, void>;

    std::map<llvm::memoir::MemOIR_Func, std::set<MemOIRInst*>> right_free_;
    std::map<llvm::memoir::MemOIR_Func, std::set<MemOIRInst*>> left_free_;

public:
    // We _always_ need to implement visitInstruction!
    void
    visitInstruction(llvm::Instruction& I)
    {
        // Do nothing.
        return;
    }

    void
    visitIndexReadInst(IndexReadInst& I)
    {
        right_free_[I.getKind()].insert(&I);
        left_free_[I.getKind()].insert(&I);
        return;
    }

    PackSet
    create_seeded_pack_set()
    {
        bool found_match = false;

        std::unordered_set<llvm::memoir::AccessInst*> matched_left;
        std::unordered_set<llvm::memoir::AccessInst*> matched_right;

        PackSet packset;
        process_index_read_seeds_(packset); // IndexReadInst

        return packset;
    }

private:
    bool
    indices_adjacent_(llvm::Value& left, llvm::Value& right)
    {
        // by convention, we will only return true if right = left + 1
        if (auto* left_int = llvm::dyn_cast<llvm::ConstantInt>(&left)) {
            if (auto* right_int = llvm::dyn_cast<llvm::ConstantInt>(&right)) {
                // only works on pairs of integer for now, scev/pattern matching is
                // future work
                return (left_int->getSExtValue() + 1 == right_int->getSExtValue());
            }
        }
        return false;
    }

    void
    process_index_read_seeds_(PackSet& ps)
    {
        for (auto& pair : left_free_) {
            if (pair.second.size() <= 0) {
                continue;
            }

            if (!IndexReadInst::classof(*pair.second.begin())) {
                // It's not an IndexReadInst of some kind
                continue;
            }

            auto kind = pair.first;
            auto& left_set = pair.second;
            auto right_iter = right_free_.find(kind);
            if (right_iter == left_free_.end() || right_iter->second.size() <= 0) {
                // We have right possibilities but no left
                continue;
            }

            auto& right_set = right_iter->second;

            std::unordered_set<MemOIRInst*> left_to_remove;
            for (auto* left_memoir_inst : left_set) {
                IndexReadInst* left_inst =
                    static_cast<IndexReadInst*>(left_memoir_inst);
                if (left_inst->getNumberOfDimensions() > 1) {
                    // We'll keep this simple for now
                    continue;
                }

                llvm::Value& left_index = left_inst->getIndexOfDimension(0);
                std::unordered_set<MemOIRInst*> right_to_remove;
                for (auto* right_memoir_inst : right_set) {
                    IndexReadInst* right_inst =
                        static_cast<IndexReadInst*>(right_memoir_inst);
                    if (right_inst->getNumberOfDimensions()
                        != left_inst->getNumberOfDimensions()) {
                        continue;
                    }

                    llvm::Value& right_index = right_inst->getIndexOfDimension(0);

                    // check that indexes are adjacent and we are reading from the same
                    // sequence
                    if (indices_adjacent_(left_index, right_index)
                        && &left_inst->getObjectOperand()
                               == &right_inst->getObjectOperand()) {
                        ps.insert(
                            &left_inst->getCallInst(), &right_inst->getCallInst()
                        );

                        // add for removal to enforce that instructions only occupy one
                        // left and right
                        left_to_remove.insert(left_memoir_inst);
                        right_to_remove.insert(right_memoir_inst);
                        break;
                    }
                }

                for (auto* right_memoir_inst : right_to_remove) {
                    right_set.erase(right_memoir_inst);
                }
            }

            for (auto* left_memoir_inst : left_to_remove) {
                left_set.erase(left_memoir_inst);
            }
        }
    }
};


class PackSetExtender {
    // Class for building out a packset from an initial seeded pack

    std::unordered_set<llvm::Instruction*> free_left_instrs;
    std::unordered_set<llvm::Instruction*> free_right_instrs;
    PackSet pack_set;

public:
    PackSetExtender(llvm::BasicBlock& bb, PackSet& p_set) {
        pack_set = p_set;

        for (llvm::Instruction& i : bb) {
            free_left_instrs.insert(&i);
            free_right_instrs.insert(&i);
        }

        // remove instructions already in packs
        for (auto it = p_set.begin(); it != p_set.end(); it++) {
            auto pack = *it;
            auto&  left_instr = pack[0];
            auto& right_instr = pack[1];
            free_left_instrs.erase(left_instr);
            free_right_instrs.erase(right_instr);
        }
    }

    void extend() {}

private:
    bool is_isomorphic(llvm::Instruction* instr_1, llvm::Instruction* instr_2) {
        // only do a basic check that both instructions are "identical"
        // other functions deal with making sure paramaters are in the correct order
        return instr_1->getOpcode() == instr_2->getOpcode();
    }

    bool is_independent(llvm::Instruction* instr_1, llvm::Instruction* instr_2) {
        // TODO find whether dependencies exist with NOELLE
        return true;
    }
};

struct SLPPass : public llvm::ModulePass {
    static char ID;

    SLPPass() : ModulePass(ID) {}

    bool
    doInitialization(llvm::Module& M) override
    {
        return false;
    }

    bool
    runOnBasicBlock(llvm::BasicBlock& BB)
    {
        PackSeeder visitor;
        for (llvm::Instruction& i : BB) {
            // llvm::memoir::println(i);
            visitor.visit(i);
        }

        llvm::memoir::println(std::string(80, '-'));

        // find packs
        PackSet packset = visitor.create_seeded_pack_set();
        llvm::memoir::println("Seeded PackSet: ", packset.dbg_string());

        // TODO: Extend the packs with use-def and def-use chains
        // P = extend_packlist(BB, P)
        PackSet extended_packs = packset; // temp

        // Combine packs into things that can be vectorized
        auto merged_packs = merge_packs(extended_packs);
        llvm::memoir::println("Merged PackSet: ", merged_packs.dbg_string());

        return false;
    }

    bool
    runOnModule(llvm::Module& M) override
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

    void
    getAnalysisUsage(llvm::AnalysisUsage& AU) const override
    {
        return;
    }
};

// Next there is code to register your pass to "opt"
char SLPPass::ID = 0;
static llvm::RegisterPass<SLPPass>
    X("memoir-vector", "Trying out SLP Vectorization in MemOIR");
} // namespace
