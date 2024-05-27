#pragma once

#include "llvm/IR/Instruction.h"
#include "llvm/Support/Casting.h"

#include <unordered_set>

inline std::unordered_set<llvm::Instruction*>
get_users_set(llvm::Instruction* inst)
{
    std::unordered_set<llvm::Instruction*> users;

    for (auto* user : inst->users()) {
        if (auto* inst_user = llvm::dyn_cast<llvm::Instruction>(user))
            users.insert(inst_user);
    }

    return users;
}
