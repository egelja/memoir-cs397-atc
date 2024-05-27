#include "pack.hpp"

size_t
Pack::index_of(llvm::Instruction* inst) const
{
    auto it = std::find(insts_.begin(), insts_.end(), inst);

    if (it == insts_.end())
        return -1;
    return std::distance(insts_.begin(), it);
}

std::string
Pack::dbg_string() const
{
    std::string s;
    llvm::raw_string_ostream ss(s);

    ss << "  (\n";

    for (auto* i : insts_)
        ss << "    " << *i << "\n";

    ss << "  )";
    return ss.str();
}
