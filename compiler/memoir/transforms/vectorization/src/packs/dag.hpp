#pragma once

#include "pack.hpp"
#include "pack_set.hpp"

#include "llvm/IR/Instruction.h"

#include <memory>
#include <unordered_map>

// forward decl
class PackDAG;

/**
 * A node in the PackSetDAG.
 */
class PackDAGNode {
    using NodeIdxMap = std::unordered_map<PackDAGNode*, size_t>;

    // the of instructions we care about
    const Pack pack_;

    // Map from
    //    idx of instruction
    //        -> pack that produces instruction operand
    //        -> index of producing instruction in producing pack
    //
    // So operands_[1][p] = 3 means the fourth instruction in pack p produces a
    // value used by the second instruction in our pack.
    std::vector<NodeIdxMap> operand_nodes_;

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

    friend class PackDAG;

private:
    PackDAGNode(Pack pack, PackDAG* parent) :
        pack_(std::move(pack)),
        operand_nodes_(pack_.size(), NodeIdxMap{}),
        parent_(parent)
    {}
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
    void init_node_op_map_(PackDAGNode& node);

    /**
     * Update operand maps for other nodes.
     */
    void update_other_op_maps_(PackDAGNode* producer_node);
};
