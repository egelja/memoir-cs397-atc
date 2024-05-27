#include "merging.hpp"

PackSet
merge_packs(PackSet ps)
{
    // fixed point algo
    bool dirty = false;

    do {
        dirty = false;

        // look at each pair of packs
        for (const auto& p1 : ps) {
            for (const auto& p2 : ps) {
                // merge if last of p1 is the same as first of p2
                if (p1.back() != p2.front())
                    continue;

                // do the merge
                Pack merged = p1;

                for (auto* inst : p2)
                    merged.append_right(inst);

                // update pack set
                ps.remove(p1);
                ps.remove(p2);

                if (p1.is_seed()) {
                    assert(p2.is_seed());
                    merged.is_seed() = true;
                }

                ps.insert(std::move(merged));

                // need to keep repeating
                dirty = true;

                // TODO this could be improved with a list of changes that we
                // apply after we analyze the entire set
                //
                // however, this works and perf is not the first priority here
                goto restart; // break both loops
            }
        }

restart:
    /* why c++ */;
    } while (dirty);

    return ps;
}
