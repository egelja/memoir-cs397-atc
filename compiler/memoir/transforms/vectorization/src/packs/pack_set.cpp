#include "pack_set.hpp"

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
