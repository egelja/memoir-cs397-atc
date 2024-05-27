#include "dag.hpp"

#include "memoir/support/Print.hpp"
#include "memoir/transforms/vectorization/src/utils/llvm.hpp"

#include <memory>
#include <unordered_set>

std::shared_ptr<PackDAGNode>
PackDAG::add_node(Pack pack)
{
    // create the node and add it to our graph
    auto node = make_node_(std::move(pack), this);

    // update instruction to node map
    size_t pack_idx = 0;

    for (auto* instr : node->pack()) {
        llvm::memoir::println("Adding instr: ", *instr);
        // instructions should not already be here
        assert(inst_to_node_.count(instr) == 0);

        // add instruction
        inst_to_node_[instr] = {node, pack_idx};
        pack_idx++;
    }

    // set up our operand map
    init_node_op_map_(*node);

    // update operand maps in other instructions
    update_other_op_maps_(node.get());

    // add the node to our graph
    nodes_.push_back(node);

    if (node->pack().is_seed())
        seeds_.push_back(node);

    return node;
}

void
PackDAG::init_node_op_map_(PackDAGNode& node)
{
    size_t pack_idx = 0;

    for (auto* instr : node.pack()) {
        // get operands map
        auto& inst_op_node_idxs = node.operand_nodes_[pack_idx];

        for (size_t i = 0; i < instr->getNumOperands(); ++i) {
            // get the operand
            auto* op_instr = llvm::dyn_cast<llvm::Instruction>(instr->getOperand(i));
            if (!op_instr)
                continue;

            // check if anyone in the graph contains this instruction
            auto it = inst_to_node_.find(op_instr);
            if (it == inst_to_node_.end())
                continue;

            auto [op_node, op_node_inst_idx] = it->second;

            // update our map
            inst_op_node_idxs[op_node.get()] = op_node_inst_idx;
        }

        // next instruction
        pack_idx++;
    }
}

void
PackDAG::update_other_op_maps_(PackDAGNode* producer_node)
{
    // find all users for each index in our node
    std::vector<std::unordered_set<llvm::Instruction*>> users_for_inst;

    for (auto* inst : producer_node->pack())
        users_for_inst.push_back(get_users_set(inst));

    // update other nodes
    for (size_t inst_idx = 0; inst_idx < users_for_inst.size(); ++inst_idx) {
        const auto& users = users_for_inst[inst_idx];

        for (auto* user : users) {
            // find the user
            auto it = inst_to_node_.find(user);
            if (it == inst_to_node_.end())
                continue;

            auto [node, node_inst_idx] = it->second;

            // update its map
            node->operand_nodes_[node_inst_idx][producer_node] = inst_idx;
        }
    }
}
