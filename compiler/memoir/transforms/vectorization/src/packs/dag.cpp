#include "dag.hpp"

#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_ostream.h"

#include "memoir/support/Print.hpp"
#include "memoir/transforms/vectorization/src/packs/pack.hpp"

#include <memory>

///////////////////////////////////////////////////////////////////////////////

PackDAGNode::PackDAGNode(Pack pack, PackDAG* parent) :
    pack_(std::move(pack)), operand_nodes_(), parent_(parent)
{
    LaneProducerMap map;
    for (size_t i = 0; i < num_lanes(); ++i)
        map.push_back({std::weak_ptr<PackDAGNode>{}, 0});

    for (size_t i = 0; i < num_operands(); ++i)
        operand_nodes_.push_back(map); // copy
}

///////////////////////////////////////////////////////////////////////////////

namespace {

void
handle_cyclical_node(PackDAGNode& node)
{
    llvm::memoir::println("Pack references itself: \n", node.pack().dbg_string());
    abort();
}

bool
skip_node_map_update(PackDAGNode* producer, PackDAGNode* consumer)
{
    return producer == consumer && producer->type() == PackType::STORE;
}

} // namespace

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
    init_node_op_map_(node);

    // update operand maps in other instructions
    update_other_op_maps_(node);

    // add the node to our graph
    nodes_.push_back(node);

    if (node->pack().is_seed())
        seeds_.push_back(node);

    return node;
}

void
PackDAG::init_node_op_map_(std::shared_ptr<PackDAGNode>& node)
{
    for (size_t op_idx = 0; op_idx < node->num_operands(); ++op_idx) {
        for (size_t lane_idx = 0; lane_idx < node->num_lanes(); ++lane_idx) {
            // get the instruction
            auto* inst = node->pack()[lane_idx];

            // get the operand
            auto* op_instr =
                llvm::dyn_cast<llvm::Instruction>(inst->getOperand(op_idx));

            if (!op_instr)
                continue;

            // check if anyone in the graph contains this instruction
            auto it = inst_to_node_.find(op_instr);
            if (it == inst_to_node_.end())
                continue;

            auto [op_node, op_node_lane] = it->second;

            // skip stores that reference themselves because of MemOIR
            if (skip_node_map_update(op_node.get(), node.get()))
                continue;

            // update our map
            if (op_node == node)
                handle_cyclical_node(*node);

            node->operand_nodes_[op_idx][lane_idx] = {op_node, op_node_lane};
        }
    }
}

void
PackDAG::update_other_op_maps_(std::shared_ptr<PackDAGNode>& node)
{
    // update other nodes
    for (size_t lane_idx = 0; lane_idx < node->num_lanes(); ++lane_idx) {
        // get the instruction
        auto* inst = node->pack()[lane_idx];

        for (auto& use : inst->uses()) {
            // deconstruct use
            size_t op_idx = use.getOperandNo();

            auto* user = llvm::dyn_cast<llvm::Instruction>(use.getUser());
            if (!user)
                continue;

            // find the user
            auto it = inst_to_node_.find(user);
            if (it == inst_to_node_.end())
                continue;

            auto [user_node, user_node_lane] = it->second;

            // skip stores that reference themselves because of MemOIR
            if (skip_node_map_update(node.get(), user_node.get()))
                continue;

            // update our map
            if (node == user_node)
                handle_cyclical_node(*node);

            // update its map
            user_node->operand_nodes_[op_idx][user_node_lane] = {node, lane_idx};
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

    // operation type
    label += "(" + pack_type_string(node.type()) + ")  ";

    // operand list
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

        // find connections
        std::unordered_map<PackDAGNode*, IndexMap> idx_maps;

        for (size_t op = 0; op < node->num_operands(); ++op) {
            for (size_t lane = 0; lane < node->num_lanes(); ++lane) {
                // get producer node and lane
                auto [prod_node_ptr, prod_node_lane] = node->operand_nodes_[op][lane];

                auto prod_node = prod_node_ptr.lock();
                if (!prod_node)
                    continue;

                // save to our index map
                std::pair<size_t, size_t> idx_pair{prod_node_lane, lane};

                if (idx_maps.count(prod_node.get()))
                    idx_maps[prod_node.get()].push_back(idx_pair);
                else
                    idx_maps[prod_node.get()] = {idx_pair};
            }
        }

        // emit edges
        for (auto& [prod_node, idx_map] : idx_maps)
            emit_edge(ss, prod_node, node.get(), idx_map);
    }

    // footer
    ss << "}"
       << "\n\n\n";

    return ss.str();
}
