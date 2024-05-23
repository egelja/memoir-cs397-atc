#include "packs.hpp"

std::string
Pack::dbg_string()

{
    bool first_iter = true;
    std::string s;
    llvm::raw_string_ostream ss(s);

    ss << "(";
    for (auto* i : instructions_) {
        if (!first_iter) {
            ss << ", ";
        }
        ss << *i;
    }
    ss << ")";
    return ss.str();
}

std::string
PackSet::dbg_string()
{
    bool first_iter = true;
    std::string str = "{\n";
    for (auto* pack : this->packs_) {
        if (!first_iter) {
            str += ", ";
        }
        str += pack->dbg_string();
        str += "\n";
        first_iter = false;
    }
    str += "}";
    return str;
}
