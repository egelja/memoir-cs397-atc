#include "extension.hpp"

#include <llvm/IR/Function.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>

#include "memoir/analysis/TypeAnalysis.hpp"
#include "memoir/ir/Instructions.hpp"
#include "memoir/support/InternalDatatypes.hpp"
#include "memoir/support/Print.hpp"

PacksetExtender::PacksetExtender(llvm::BasicBlock& bb, PackSet* p_set, PDG* graph)
{
    pack_set = p_set;
    fdg = graph;

    for (llvm::Instruction& i : bb) {
        free_left_instrs.insert(&i);
        free_right_instrs.insert(&i);
    }

    // remove instructions already in packs
    for (auto it = p_set->begin(); it != p_set->end(); it++) {
        auto pack = *it;
        auto& left_instr = pack[0];
        auto& right_instr = pack[1];
        free_left_instrs.erase(left_instr);
        free_right_instrs.erase(right_instr);
    }
}

void PacksetExtender::extend()
{
    bool changed = true;

    while (changed) {
        changed = false;

        for (auto it = pack_set->begin(); it != pack_set->end(); it++) {
            auto pack = *it;

            if (follow_def_uses(pack) || follow_use_defs(pack)) {
                changed = true;

                // stop early since we modified pack_set
                break;
            }
        }
    }
}

bool PacksetExtender::is_isomorphic(llvm::Instruction* instr_1, llvm::Instruction* instr_2)
{
    // only do a basic check that both instructions are "identical"
    // other functions deal with making sure paramaters are in the correct order
    return instr_1->getOpcode() == instr_2->getOpcode();
}

bool PacksetExtender::is_independent(llvm::Instruction* instr_1, llvm::Instruction* instr_2)
{
    bool dependency_exists = false;

    auto instr_1_iter_f = [&dependency_exists,
                           instr_2](Value* src, DGEdge<Value, Value>* dep) {
        // dependency exists if there is an edge between instr_1 and instr_2
        if (dep->getDst() == instr_2) {
            dependency_exists = true;
            return true;
        }

        return false;
    };

    fdg->iterateOverDependencesFrom(instr_1, true, true, true, instr_1_iter_f);

    auto instr_2_iter_f = [&dependency_exists,
                           instr_1](Value* src, DGEdge<Value, Value>* dep) {
        // dependency exists if there is an edge between instr_2 and instr_1
        if (dep->getDst() == instr_1) {
            dependency_exists = true;
            return true;
        }

        return false;
    };

    fdg->iterateOverDependencesFrom(instr_2, true, true, true, instr_2_iter_f);

    return !dependency_exists;
}

bool PacksetExtender::instrs_can_pack(llvm::Instruction* instr_1, llvm::Instruction* instr_2)
{
    // check that instrs are not in another pack already
    if ((free_left_instrs.find(instr_1) == free_left_instrs.end())
        || (free_right_instrs.find(instr_2) == free_right_instrs.end())) {
        return false;
    }

    return is_isomorphic(instr_1, instr_2) && is_independent(instr_1, instr_2);
}

bool PacksetExtender::follow_use_defs(Pack p)
{
    // returns whether new values were added to pack_set
    llvm::Instruction* left_instr = p[0];
    llvm::Instruction* right_instr = p[1];

    // sanity check
    assert(is_isomorphic(left_instr, right_instr));

    bool changed = false;
    for (int i = 0; i < left_instr->getNumOperands(); i++) {
        // grab operands in the same position
        auto* op_1 = left_instr->getOperand(i);
        auto* op_2 = right_instr->getOperand(i);

        // check if definition of operands is packable
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

bool PacksetExtender::follow_def_uses(Pack p)
{
    // returns whether new values were added to pack set
    llvm::Instruction* left_instr = p[0];
    llvm::Instruction* right_instr = p[1];

    bool changed = false;

    // look for uses of left/right instr that occupy the same operand position
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

            // we can't have a pack with two of the same instr
            if (left_user_instr == right_user_instr) {
                continue;
            }

            if (left_user_instr->getNumOperands()
                != right_user_instr->getNumOperands()) {
                continue;
            }

            // no easy way to grab the operand position of a use
            for (int i = 0; i < left_user_instr->getNumOperands(); i++) {
                auto op_1 = left_user_instr->getOperand(i);
                auto op_2 = right_user_instr->getOperand(i);

                if (auto* op_instr_1 = dyn_cast<llvm::Instruction>(op_1)) {
                    if (auto* op_instr_2 = dyn_cast<llvm::Instruction>(op_2)) {
                        if (op_instr_1 == left_instr && op_instr_2 == right_instr) {
                            if (instrs_can_pack(
                                    left_user_instr, right_user_instr
                                )) {
                                pack_set->insert(left_user_instr, right_user_instr);

                                free_left_instrs.erase(left_user_instr);
                                free_right_instrs.erase(right_user_instr);
                                changed = true;

                                // in the paper, this function chooses the pair of
                                // instructions that produce the largest savings.
                                // since we don't have a cost model, we return the
                                // first found
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

