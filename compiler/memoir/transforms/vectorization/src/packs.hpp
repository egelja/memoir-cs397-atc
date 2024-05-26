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

    auto
    begin()
    {
        return instructions_.begin();
    }

    const auto
    begin() const
    {
        return instructions_.begin();
    }

    auto
    end()
    {
        return instructions_.end();
    }

    const auto
    end() const
    {
        return instructions_.end();
    }

    auto&
    operator[](size_t idx)
    {
        return instructions_.at(idx);
    }

    const auto&
    operator[](size_t idx) const
    {
        return instructions_.at(idx);
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

    std::unordered_set<Pack*> get_packs() {
        return packs_;
    }
};
