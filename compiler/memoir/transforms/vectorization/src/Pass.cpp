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
//#include "memoir/transforms/vectorization/src/packs/merging.hpp"
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


class PacksetExtender {
    // Class for building out a packset from an initial seeded pack

    std::unordered_set<llvm::Instruction*> free_left_instrs;
    std::unordered_set<llvm::Instruction*> free_right_instrs;
    PackSet* pack_set;

public:
    PacksetExtender(llvm::BasicBlock& bb, PackSet* p_set) {
        pack_set = p_set;

        for (llvm::Instruction& i : bb) {
            free_left_instrs.insert(&i);
            free_right_instrs.insert(&i);
        }

        // remove instructions already in packs
        for (auto it = p_set->begin(); it != p_set->end(); it++) {
            auto pack = *it;
            auto&  left_instr = pack[0];
            auto& right_instr = pack[1];
            free_left_instrs.erase(left_instr);
            free_right_instrs.erase(right_instr);
        }
    }

    void extend() {
        bool changed = true;

        while (changed) {
            changed = false;

            for (auto it = pack_set->begin(); it != pack_set->end(); it++) {
                auto pack = *it;

                if (follow_def_uses(pack) || follow_use_defs(pack)) {
                    changed = true;
                    break;
                }
            }
        }
    }

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

    bool instrs_can_pack(llvm::Instruction* instr_1, llvm::Instruction* instr_2) {
        // check that instrs are not in another pack already
        if ((free_left_instrs.find(instr_1) == free_left_instrs.end()) || 
            (free_right_instrs.find(instr_2) == free_right_instrs.end())) {
            return false;
        }

        return is_isomorphic(instr_1, instr_2) && is_independent(instr_1, instr_2);
    }

    bool follow_use_defs(Pack p) {
        // returns whether new values were added to pack_set
        llvm::Instruction* left_instr = p[0];
        llvm::Instruction* right_instr = p[1];

        // sanity check
        assert(is_isomorphic(left_instr, right_instr));

        bool changed = false;
        for (int i = 0; i < left_instr->getNumOperands(); i++) {
            auto* op_1 = left_instr->getOperand(i);
            auto* op_2 = right_instr->getOperand(i);

            if (auto* op_instr_1 = llvm::dyn_cast<llvm::Instruction>(op_1)) {
                if (auto* op_instr_2 = llvm::dyn_cast<llvm::Instruction>(op_2)) {
                    if (instrs_can_pack(op_instr_1, op_instr_2)) {
                        pack_set->insert(op_instr_1, op_instr_2);

                        free_left_instrs.erase(op_instr_1);
                        free_right_instrs.erase(op_instr_2);
                        changed = true;
                    }
                }
            }
        }

        return changed;
    }

    bool follow_def_uses(Pack p) {
        // returns whether new values were added to pack set
        llvm::Instruction* left_instr = p[0];
        llvm::Instruction* right_instr = p[1];

        bool changed = false;
        for (auto left_user : left_instr->users()) {
            auto* left_user_instr = dyn_cast<llvm::Instruction>(left_user);
            if (left_user_instr == NULL) {
                continue;
            }

            for (auto right_user : right_instr->users()) {
                auto* right_user_instr = dyn_cast<llvm::Instruction>(right_user);
                if (right_user_instr == NULL) {
                    continue;
                }

                if (left_user_instr == right_user_instr) {
                    continue;
                }

                if (left_user_instr->getNumOperands() != right_user_instr->getNumOperands()) {
                    continue;
                }

                for (int i = 0; i < left_user_instr->getNumOperands(); i++) {
                    auto op_1 = left_user_instr->getOperand(i);
                    auto op_2 = right_user_instr->getOperand(i);

                    if (auto* op_instr_1 = dyn_cast<llvm::Instruction>(op_1)) {
                        if (auto* op_instr_2 = dyn_cast<llvm::Instruction>(op_2)) {
                            if (op_instr_1 == left_instr && op_instr_2 == right_instr) {
                                if (instrs_can_pack(left_user_instr, right_user_instr)) {
                                    pack_set->insert(left_user_instr, right_user_instr);
                                    
                                    free_left_instrs.erase(left_user_instr);
                                    free_right_instrs.erase(right_user_instr);
                                    changed = true;

                                    // in the paper, this function chooses the pair of instructions that produce the 
                                    // largest savings. since we don't have a cost model, we return the first found
                                    return changed;
                                }
                            }
                        }
                    }
                }
            }
        }

        return changed;

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
        PacksetExtender extender(BB, &packset);
        extender.extend();
        llvm::memoir::println("Extended Packset: ", packset.dbg_string());
        // P = extend_packlist(BB, P)
        //PackSet extended_packs = packset; // temp


        // Combine packs into things that can be vectorized
        //auto merged_packs = merge_packs(extended_packs);
        //llvm::memoir::println("Merged PackSet: ", merged_packs.dbg_string());

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
