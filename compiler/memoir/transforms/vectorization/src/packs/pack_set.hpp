#pragma once

#include "pack.hpp"

#include <unordered_set>

class PackSet {
protected:
    std::unordered_set<Pack> packs_;

public:
    /**
     * Create a new pack with two instructions and insert it into this pack set.
     *
     * @param left Left instruction in the pack.
     * @param right Right instruction in the pack.
     */
    void insert(llvm::Instruction* left, llvm::Instruction* right);

    /**
     * Insert an existing pack into this pack set.
     *
     * @param p Pack to insert.
     */
    void insert(Pack p) { packs_.insert(std::move(p)); }

    /**
     * Remove a pack from this pack set.
     *
     * @param p pack to remove.
     */
    void remove(const Pack& p) { packs_.erase(p); }

    /**
     * Get a string representing this pack set.
     */
    std::string dbg_string() const;

    /////////// C++ boilerplate stuff ////////////

    auto begin() { return packs_.begin(); }

    auto begin() const { return packs_.begin(); }

    auto end() { return packs_.end(); }

    auto end() const { return packs_.end(); }

    bool operator==(const PackSet& other) const { return packs_ == other.packs_; }

    bool operator!=(const PackSet& other) const { return !(*this == other); }
};
