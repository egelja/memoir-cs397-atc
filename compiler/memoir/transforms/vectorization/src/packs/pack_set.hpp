#pragma once

#include "pack.hpp"

#include <unordered_set>

class PackSet {
protected:
    std::unordered_set<Pack> packs_;

public:
    void
    insert(llvm::Instruction* left, llvm::Instruction* right)
    {
        Pack pair;

        pair.append_right(left);
        pair.append_right(right);

        packs_.insert(std::move(pair));
    }

    std::string dbg_string() const;

    bool
    operator==(const PackSet& other) const
    {
        return packs_ == other.packs_;
    }

    bool
    operator!=(const PackSet& other) const
    {
        return !(*this == other);
    }
};
