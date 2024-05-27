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
    // the of instructions we care about
    const Pack pack_;

    // Map from
    //    idx of instruction
    //        -> pack that produces instruction operand
    //        -> index of producing instruction in producing pack
    //
    // So operands_[1][p] = 3 means the fourth instruction in pack p produces a
    // value used by the second instruction in our pack.
    std::vector<std::unordered_map<PackDAGNode*, size_t>> operands_;

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
    PackDAGNode(Pack pack, PackDAG* parent);
};

/**
 * A DAG of packed instructions.
 */
class PackDAG {
    using InstructionSet = std::unordered_set<llvm::Instruction*>;

    // nodes stored in reverse topological order
    // the seed pack goes first
    // TODO do we need to support multiple seed packs???
    std::vector<std::shared_ptr<PackDAGNode>> nodes_;

    // map from instruction to the node containing that instruction
    std::unordered_map<llvm::Instruction*, std::shared_ptr<PackDAGNode>> inst_to_node_;

public:
    /**
     * Create a PackDAG from a packset and seed nodes.
     */
    PackDAG(const PackSet& packset, const InstructionSet& seeds);

    /**
     * Get our seed nodes.
     */
    std::shared_ptr<PackDAGNode> seeds() const { return nodes_.front(); }

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

private:
    /**
     * Find the seed pack in a pack set.
     */
    Pack find_seed_pack_(const PackSet& packset, const InstructionSet& seeds);

    /**
     * Update argument map for all instructions.
     */
    void update_op_maps_(PackDAGNode* producer_node);
};
