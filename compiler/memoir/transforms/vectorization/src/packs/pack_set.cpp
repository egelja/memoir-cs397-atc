#include "pack_set.hpp"

void
PackSet::insert(llvm::Instruction* left, llvm::Instruction* right)
{
    Pack pair;

    pair.append_right(left);
    pair.append_right(right);

    packs_.insert(std::move(pair));
}

std::string
PackSet::dbg_string() const
{
    if (packs_.empty())
        return "{}";

    std::string str = "{\n";

    for (const auto& pack : packs_)
        str += pack.dbg_string() + "\n";

    str += "}";
    return str;
}
