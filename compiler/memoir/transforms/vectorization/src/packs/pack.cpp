#include "pack.hpp"

std::string
Pack::dbg_string() const
{
    std::string s;
    llvm::raw_string_ostream ss(s);

    ss << "  (\n";

    for (auto* i : instructions_)
        ss << "    " << *i << "\n";

    ss << "  )";
    return ss.str();
}
