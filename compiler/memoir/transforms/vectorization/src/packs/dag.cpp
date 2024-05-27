#include "dag.hpp"

#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_ostream.h"

#include "memoir/support/Print.hpp"
#include "memoir/transforms/vectorization/src/utils/llvm.hpp"

#include <memory>
#include <ostream>
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

//////////////////////////////////////////////////////////////////////////////

namespace {

using IndexMap = std::vector<std::pair<size_t, size_t>>;

/**
 * ONLY FOR DEBUGGING PURPOSES.
 */
std::string
get_number_of_instruction(const llvm::Instruction* inst)
{
    std::string str;
    llvm::raw_string_ostream ss(str);

    ss << *inst;
    ss.flush();

    // now extract the %XX bit
    // first two chars are always spaces
    std::string res;

    for (size_t i = 2; i < str.size() && str[i] != ' '; ++i)
        res += str[i];

    return res;
}

std::string
node_name(PackDAGNode* node)
{
    return std::string("node") + std::to_string(reinterpret_cast<uintptr_t>(node));
}

std::string
node_label(PackDAGNode& node)
{
    std::string label;

    for (const auto* inst : node.pack())
        label += get_number_of_instruction(inst) + ", ";

    label.erase(label.size() - 2); // drop last ', '
    return label;
}

void
emit_node_decl(llvm::raw_string_ostream& ss, PackDAGNode* node)
{
    // emit node name
    ss << node_name(node) << " [";

    // emit label
    ss << "label=\"" << node_label(*node) << "\"";

    // emit color for seed nodes
    if (node->is_seed())
        ss << ", color=green";

    // finish node
    ss << ", shape=box];"
       << "\n";
}

void
emit_edge(
    llvm::raw_string_ostream& ss,
    PackDAGNode* src,
    PackDAGNode* dest,
    const IndexMap& idx_map
)
{
    // edge header
    ss << node_name(src) << " -> " << node_name(dest);

    // label
    ss << " [label=\"{";

    for (const auto& [x, y] : idx_map)
        ss << "(" << x << ", " << y << ") ";

    // finish up
    ss.str().pop_back(); // drop trailing space, little jank (XXX)

    ss << "}\"];"
       << "\n";
}

} // namespace

std::string
PackDAG::to_graphviz() const
{
    std::string str;
    llvm::raw_string_ostream ss(str);

    // header
    ss << "\n\n"
       << "digraph G {"
       << "\n";

    // nodes
    for (auto& node : nodes_) {
        // emit the node declaration
        emit_node_decl(ss, node.get());

        // find nodes we connect to
        std::unordered_set<PackDAGNode*> connected_nodes;

        for (const auto& node_idx_map : node->operand_nodes_)
            for (const auto& [node, idx] : node_idx_map)
                connected_nodes.insert(node);

        // emit connections
        for (auto* op_node : connected_nodes) {
            // get idx map
            IndexMap idx_map;

            for (size_t i = 0; i < node->operand_nodes_.size(); ++i) {
                auto it = node->operand_nodes_[i].find(op_node);

                if (it != node->operand_nodes_[i].end())
                    idx_map.emplace_back(it->second, i); // src idx, dest idx
            }

            // emit edge
            emit_edge(ss, op_node, node.get(), idx_map);
        }
    }

    // footer
    ss << "}"
       << "\n\n\n";

    return ss.str();
}
