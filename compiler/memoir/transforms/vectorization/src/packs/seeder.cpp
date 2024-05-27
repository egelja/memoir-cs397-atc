#include "seeder.hpp"

PackSeeder::PackSeeder(Noelle* n) {
    noelle = n;
}

void PackSeeder::visitIndexReadInst(IndexReadInst& I)
{
    right_free_[I.getKind()].insert(&I);
    left_free_[I.getKind()].insert(&I);
    return;
}

PackSet PackSeeder::create_seeded_pack_set()
{
    bool found_match = false;

    std::unordered_set<llvm::memoir::AccessInst*> matched_left;
    std::unordered_set<llvm::memoir::AccessInst*> matched_right;

    PackSet packset;
    process_index_read_seeds_(packset); // IndexReadInst

    return packset;
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
                    && &left_inst->getObjectOperand()
                           == &right_inst->getObjectOperand()) {
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
