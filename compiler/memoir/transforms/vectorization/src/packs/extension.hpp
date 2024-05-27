#pragma once

#include "pack.hpp"
#include "pack_set.hpp"

#include "llvm/IR/BasicBlock.h"
#include "noelle/core/Noelle.hpp"

using namespace arcana::noelle;

class PacksetExtender {
    // Class for building out a packset from an initial seeded pack
    PDG* fdg;

    std::unordered_set<llvm::Instruction*> free_left_instrs;
    std::unordered_set<llvm::Instruction*> free_right_instrs;
    PackSet* pack_set;

public:
    PacksetExtender(llvm::BasicBlock& bb, PackSet* p_set, PDG* graph);

    void extend();

private:
    bool is_isomorphic(llvm::Instruction* instr_1, llvm::Instruction* instr_2);

    bool is_independent(llvm::Instruction* instr_1, llvm::Instruction* instr_2);

    bool instrs_can_pack(llvm::Instruction* instr_1, llvm::Instruction* instr_2);

    bool follow_use_defs(Pack p);

    bool follow_def_uses(Pack p);
};
