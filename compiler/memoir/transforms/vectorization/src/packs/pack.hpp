#pragma once

#include "llvm/IR/Instruction.h"

#include <deque>

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

    std::string dbg_string() const;

    bool
    operator==(const Pack& other) const
    {
        return instructions_ == other.instructions_;
    }
};

///////////////////////////////////////////////////////////////////////////////

template <>
struct std::hash<Pack> {
    std::size_t
    operator()(const Pack& pack) const noexcept
    {
        size_t seed = 0;

        for (const auto& instr : pack)
            hash_combine(seed, instr);

        return seed;
    }
};
