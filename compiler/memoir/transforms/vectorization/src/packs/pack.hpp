#pragma once

#include "llvm/IR/Instruction.h"

#include <deque>

enum class PackType {
    // Memory
    LOAD,
    STORE,

    // Computation
    ADD,
};

class Pack {
protected:
    std::deque<llvm::Instruction*> insts_;
    bool is_seed_;

public:
    Pack() : insts_(), is_seed_(false) {}

    explicit Pack(bool is_seed) : insts_(), is_seed_(is_seed) {}

    void append_right(llvm::Instruction* i) { insts_.push_back(i); }

    void append_left(llvm::Instruction* i) { insts_.push_front(i); }

    void pop_right() { insts_.pop_back(); }

    void pop_left() { insts_.pop_front(); }

    size_t index_of(llvm::Instruction* inst) const;

    bool& is_seed() { return is_seed_; }

    bool is_seed() const { return is_seed_; }

    /**
     * How many arguments does the instruction of this pack have?
     */
    size_t num_operands() const { return insts_[0]->getNumOperands(); }

    /**
     * How many instructions (lanes) are in this pack?
     */
    size_t num_lanes() const { return insts_.size(); }

    /**
     * Get the type of this pack.
     */
    PackType type() const;

    std::string dbg_string() const;

    ////////// C++ boilerplate stuff //////////

    auto* front() const { return insts_.front(); }

    auto* back() const { return insts_.back(); }

    auto begin() { return insts_.begin(); }

    const auto begin() const { return insts_.begin(); }

    auto end() { return insts_.end(); }

    const auto end() const { return insts_.end(); }

    size_t size() const { return insts_.size(); };

    auto& operator[](size_t idx) { return insts_.at(idx); }

    const auto& operator[](size_t idx) const { return insts_.at(idx); }

    bool operator==(const Pack& other) const { return insts_ == other.insts_; }

    bool operator!=(const Pack& other) const { return !(*this == other); }
};

std::string pack_type_string(PackType type);

///////////////////////////////////////////////////////////////////////////////

template <>
struct std::hash<Pack> {
    std::size_t operator()(const Pack& pack) const noexcept
    {
        size_t seed = 0;

        for (const auto& instr : pack)
            hash_combine(seed, instr);

        return seed;
    }
};
