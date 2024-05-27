#include "seeder.hpp"

PackSeeder::PackSeeder(PDG* graph) {
    fdg = graph;
}

void PackSeeder::visitIndexReadInst(IndexReadInst& I)
{
    right_free_[I.getKind()].insert(&I);
    left_free_[I.getKind()].insert(&I);
    return;
}

void PackSeeder::visitIndexWriteInst(IndexWriteInst& I) {
    write_right_free_[I.getKind()].insert(&I);
    write_left_free_[I.getKind()].insert(&I);
    return;
}

PackSet PackSeeder::create_seeded_pack_set()
{
    PackSet packset;
    process_index_read_seeds_(packset); // IndexReadInst
    process_index_write_seeds_(packset); // IndexWriteInst

    return packset;
}

bool PackSeeder::is_independent(llvm::Instruction* instr_1, llvm::Instruction* instr_2) {
    bool dependency_exists = false;

    auto instr_1_iter_f = [&dependency_exists,
                           instr_2](Value* src, DGEdge<Value, Value>* dep) {

        if (src == dep->getDst()) {
            return true;
        }

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

bool PackSeeder::indices_adjacent_(llvm::Value& left, llvm::Value& right)
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

void PackSeeder::process_index_read_seeds_(PackSet& ps)
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
                    && (&left_inst->getObjectOperand()
                           == &right_inst->getObjectOperand())
                    && is_independent(&left_inst->getCallInst(), &right_inst->getCallInst())) {
                    ps.insert(
                        &left_inst->getCallInst(), &right_inst->getCallInst(), true
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

void PackSeeder::process_index_write_seeds_(PackSet& ps) {
    for (auto& pair : write_left_free_) {
        if (pair.second.size() <= 0) {
            continue;
        }

        if (!IndexWriteInst::classof(*pair.second.begin())) {
            // It's not an IndexReadInst of some kind
            continue;
        }

        auto kind = pair.first;
        auto& left_set = pair.second;
        auto right_iter = write_right_free_.find(kind);
        if (right_iter == write_left_free_.end() || right_iter->second.size() <= 0) {
            // We have right possibilities but no left
            continue;
        }

        auto& right_set = right_iter->second;

        std::unordered_set<MemOIRInst*> left_to_remove;
        for (auto* left_memoir_inst : left_set) {
            IndexWriteInst* left_inst =
                static_cast<IndexWriteInst*>(left_memoir_inst);
            if (left_inst->getNumberOfDimensions() > 1) {
                // We'll keep this simple for now
                continue;
            }

            llvm::Value& left_index = left_inst->getIndexOfDimension(0);
            std::unordered_set<MemOIRInst*> right_to_remove;
            for (auto* right_memoir_inst : right_set) {
                IndexWriteInst* right_inst =
                    static_cast<IndexWriteInst*>(right_memoir_inst);
                if (right_inst->getNumberOfDimensions()
                    != left_inst->getNumberOfDimensions()) {
                    continue;
                }

                llvm::Value& right_index = right_inst->getIndexOfDimension(0);

                // check that right_inst writes to modified collection produced by left_inst
                // this means that collection is unmodified between writes
                // not sure if we need to do dependence analysis?
                if (indices_adjacent_(left_index, right_index) &&
                    &left_inst->getCallInst() == &right_inst->getObjectOperand()) {
                    ps.insert(
                        &left_inst->getCallInst(), &right_inst->getCallInst(), true
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
