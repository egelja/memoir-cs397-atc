#pragma once

#include "pack.hpp"

#include "llvm/IR/Instruction.h"

#include <memory>
#include <set>
#include <unordered_map>

// forward decl
class PackDAG;

/**
 * A node in the PackSetDAG.
 */
class PackDAGNode {
public:
    using NodePtr = std::weak_ptr<PackDAGNode>;

    // info about the producer of some data
    struct producer_info_t {
        NodePtr node;    // who produced this data
        size_t node_idx; // which lane is the data in?
    };

    // Who produces the data for each lane in this pack?
    using LaneProducerMap = std::vector<producer_info_t>;

private:
    // the of instructions we care about
    const Pack pack_;

    // Map from op_idx -> instr_index -> (producing_pack, pp_idx)
    //
    // So operand_nodes_[0][1] = (p, 3) means pack node p produces operand 0 for
    // instruction 1 in lane 3
    std::vector<LaneProducerMap> operand_nodes_;

    // producer and consumer sets
    //
    // producers create values we use
    // consumers use our values
    //
    // have to be ordered sets because you can't hash a weak pointer
    std::set<NodePtr, std::owner_less<NodePtr>> producers_;
    std::set<NodePtr, std::owner_less<NodePtr>> consumers_;

    // parent node
    PackDAG* parent_;

public:
    /**
     * Get the pack of this node.
     */
    const Pack& pack() const { return pack_; }

    /**
     * Get the parent dag of this pack node.
     */
    PackDAG* parent() const { return parent_; }

    /**
     * Get the type of this pack.
     */
    PackType type() const { return pack_.type(); }

    /**
     * Get if this node's pack is a seed pack.
     */
    bool is_seed() const { return pack_.is_seed(); }

    /**
     * How many lanes (instructions) are in this pack.
     */
    size_t num_lanes() const { return pack_.num_lanes(); }

    /**
     * How many arguments does the instruction of this pack have?
     */
    size_t num_operands() const { return pack_.num_operands(); }

    /**
     * Get the nodes that produce data used by us.
     */
    const auto& producers() const { return producers_; }

    /**
     * Get direct predecessors (parents) of our node.
     */
    const auto& parents() const { return producers_; }

    /**
     * Get the nodes who consume our data.
     */
    const auto& consumers() const { return consumers_; }

    /**
     * Get direct successors (children) of our node.
     */
    const auto& children() const { return consumers_; }

    /**
     * Get the producers for the ith operand of each instruction in this pack.
     *
     * @param i Which operand's producers do we want?
     *
     * @returns A map from lane (instruction index) to producer information.
     */
    const LaneProducerMap& op_producers(size_t i) const { return operand_nodes_[i]; }

    friend class PackDAG;

    /////////////////////////////////

    size_t size() const { return pack_.size(); }

    auto* operator[](size_t idx) const { return pack_[idx]; }

    auto* front() const { return pack_.front(); }

    auto* back() const { return pack_.back(); }

    const auto begin() const { return pack_.begin(); }

    const auto end() const { return pack_.end(); }

private:
    PackDAGNode(Pack pack, PackDAG* parent);
};

/**
 * A DAG of packed instructions.
 */
class PackDAG {
public:
    struct instr_info_t {
        std::shared_ptr<PackDAGNode> node; // node containing the instruction
        size_t idx;                        // index within the node
    };

private:
    // nodes stored in topological order
    std::vector<std::shared_ptr<PackDAGNode>> nodes_;
    std::vector<std::shared_ptr<PackDAGNode>> seeds_;

    // map from instruction to the node containing that instruction
    std::unordered_map<llvm::Instruction*, instr_info_t> inst_to_node_;

public:
    /**
     * Create an empty PackDAG.
     */
    PackDAG() {}

    /**
     * Get our seed nodes.
     */
    const auto& seeds() const { return seeds_; }

    /**
     * Get all the nodes in this graph.
     */
    const auto& node() const { return nodes_; }

    /**
     * Add a node to the graph.
     *
     * @param pack The packed instructions in this node.
     *
     * @returns A pointer to the newly created graph node.
     */
    std::shared_ptr<PackDAGNode> add_node(Pack pack);

    /**
     * Convert this graph to a GraphViz representation that can be viewed.
     */
    std::string to_graphviz() const;

    /**
     * Get a debugging string representing this graph.
     */
    std::string dbg_string() const { return to_graphviz(); }

    ///////////// C++ Boilerplate /////////////

    auto begin() { return nodes_.rbegin(); }

    auto begin() const { return nodes_.rbegin(); }

    auto end() { return nodes_.rend(); }

    auto end() const { return nodes_.rend(); }

    size_t size() const { return nodes_.size(); }

private:
    /**
     * Make a new graph node.
     *
     * Friend classes don't work with make_shared.
     */
    template <class... Args>
    std::shared_ptr<PackDAGNode> make_node_(Args... args) const
    {
        auto* node = new PackDAGNode(std::forward<Args>(args)...);
        return std::shared_ptr<PackDAGNode>(node);
    }

    /**
     * Initialize the operand map of a new node.
     */
    void init_node_op_map_(std::shared_ptr<PackDAGNode>& node);

    /**
     * Update operand maps for other nodes.
     */
    void update_other_op_maps_(std::shared_ptr<PackDAGNode>& node);
};
