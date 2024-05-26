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

    void
    insert(Pack p)
    {
        packs_.insert(std::move(p));
    }

    void
    remove(const Pack& pack)
    {
        packs_.erase(pack);
    }

    std::string dbg_string() const;

    /////////// C++ boilerplate stuff ////////////
    auto
    begin()
    {
        return packs_.begin();
    }

    auto
    begin() const
    {
        return packs_.begin();
    }

    auto
    end()
    {
        return packs_.end();
    }

    auto
    end() const
    {
        return packs_.end();
    }

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
