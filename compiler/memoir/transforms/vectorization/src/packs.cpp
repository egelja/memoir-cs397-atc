#include "packs.hpp"

std::string
Pack::dbg_string()

{
    std::string s;
    llvm::raw_string_ostream ss(s);

    ss << "  (\n";

    for (auto* i : instructions_)
        ss << "    " << *i << "\n";

    ss << "  )";
    return ss.str();
}

std::string
PackSet::dbg_string()
{
    std::string str = "{\n";

    for (auto* pack : this->packs_)
        str += pack->dbg_string() + "\n";

    str += "}";
    return str;
}
