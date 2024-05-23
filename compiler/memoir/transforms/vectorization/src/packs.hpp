#include "llvm/IR/Instruction.h"

#include <deque>
#include <unordered_set>

class Pack {
protected:
    std::deque<llvm::Instruction*> instructions_;

public:
    void
    append_right(llvm::Instruction* i)
    {
        instructions_.push_back(i);
    }

    void
    append_left(llvm::Instruction* i)
    {
        instructions_.push_front(i);
    }

    std::string dbg_string();
};

class PackSet {
protected:
    // TODO we should make this a set of shared pointers
    std::unordered_set<Pack*> packs_;

public:
    ~PackSet(void)
    {
        for (auto* pack : packs_) {
            delete pack;
        }
    }

    void
    insert(llvm::Instruction* left, llvm::Instruction* right)
    {
        Pack* pair = new Pack();
        pair->append_right(left);
        pair->append_right(right);
        packs_.insert(pair);
    }

    std::string dbg_string();
};
