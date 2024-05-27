#pragma once

#include "llvm/IR/Instruction.h"

#include <deque>

class Pack {
protected:
    std::deque<llvm::Instruction*> insts_;
    bool is_seed_;

public:
    Pack() : insts_(), is_seed_(false) {}

    explicit Pack(bool is_seed) : insts_(), is_seed_(is_seed) {}

    void append_right(llvm::Instruction* i) { insts_.push_back(i); }

    void append_left(llvm::Instruction* i) { insts_.push_front(i); }

    size_t index_of(llvm::Instruction* inst) const;

    bool& is_seed() { return is_seed_; }

    bool is_seed() const { return is_seed_; }

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
