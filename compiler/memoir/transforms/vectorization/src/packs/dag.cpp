#include "dag.hpp"

#include "llvm/Support/Casting.h"

#include "memoir/support/Print.hpp"
#include "memoir/transforms/vectorization/src/utils/llvm.hpp"

#include <memory>

Pack
PackDAG::find_seed_pack_(const PackSet& packset, const InstructionSet& seeds)
{
    for (const auto& pack : packset) {
        // check if pack contains one of the seeds
        if (seeds.count(pack.front()) == 0)
            continue;

        // if the pack contain one seed, it will contain all seeds
        // TODO is this a valid assumption?
        for (auto* inst : pack)
            assert(seeds.count(inst) == 1);

        // found the pack we want
        return pack;
    }
}

PackDAG::PackDAG(const PackSet& packset, const InstructionSet& seeds)
{
    // create our seed node
    auto seed_pack = find_seed_pack_(packset, seeds);
    auto seed_node = std::make_shared<PackDAGNode>(std::move(seed_pack), this);

    // add the seed pack to our graph
    nodes_.push_back(seed_node);
}

std::shared_ptr<PackDAGNode>
PackDAG::add_node(Pack pack)
{
    // create the node and add it to our graph
    auto node = std::make_shared<PackDAGNode>(std::move(pack), this);

    // update operand maps in other instructions
    update_op_maps_(node.get());

    // update instruction to node map
    for (auto* instr : node->pack()) {
        assert(inst_to_node_.count(instr) == 0);
        inst_to_node_[instr] = node;
    }

    // add the node to our graph
    nodes_.push_back(node);
    return node;
}

void
PackDAG::update_op_maps_(PackDAGNode* producer_node)
{
    // find all users for each index in our node
}
