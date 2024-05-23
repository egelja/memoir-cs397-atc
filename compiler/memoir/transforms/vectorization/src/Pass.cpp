#include <iostream>
#include <string>

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "memoir/ir/Instructions.hpp"

#include "memoir/analysis/TypeAnalysis.hpp"

#include "memoir/support/InternalDatatypes.hpp"

#include "memoir/utility/FunctionNames.hpp"
#include "memoir/utility/Metadata.hpp"

/*
 * Author(s): Kevin Hayes
 */

namespace {

using namespace llvm::memoir;

class Pack {
protected:
  std::vector<llvm::Instruction *> instructions;

public:
  void appendRight(llvm::Instruction *i) {
    instructions.push_back(i);
  }
  void appendLeft(llvm::Instruction *i) {
    instructions.insert(instructions.begin(), i);
  }

  std::string dbg_string(void) {
    bool first_iter = true;
    std::string s;
    llvm::raw_string_ostream ss(s);
    ss << "(";
    for (auto *i : instructions) {
      if (!first_iter) {
        ss << ", ";
      }
      ss << *i;
    }
    ss << ")";
    return ss.str();
  }
};

class PackSet {
protected:
  std::unordered_set<Pack *> packs;

public:
  ~PackSet(void) {
    for (auto *pack : packs) {
      delete pack;
    }
  }

  void insertPair(llvm::Instruction *left, llvm::Instruction *right) {
    Pack *pair = new Pack();
    pair->appendRight(left);
    pair->appendRight(right);
    packs.insert(pair);
  }

  std::string dbg_string(void) {
    bool first_iter = true;
    std::string str = "{\n";
    for (auto *pack : this->packs) {
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
};

class PackSeeder : public llvm::memoir::InstVisitor<PackSeeder, void> {
  // In order for the wrapper to work, we need to declare our parent classes as
  // friends.
  friend class llvm::memoir::InstVisitor<PackSeeder, void>;
  friend class llvm::InstVisitor<PackSeeder, void>;

  std::map<llvm::memoir::MemOIR_Func, std::set<MemOIRInst *>> right_free;
  std::map<llvm::memoir::MemOIR_Func, std::set<MemOIRInst *>> left_free;

public:
  // We _always_ need to implement visitInstruction!
  void visitInstruction(llvm::Instruction &I) {
    // Do nothing.
    return;
  }

  void visitIndexReadInst(IndexReadInst &I) {
    this->right_free[I.getKind()].insert(&I);
    this->left_free[I.getKind()].insert(&I);
    return;
  }

  bool indexesAreAdjacent(llvm::Value &left, llvm::Value &right) {
    // by convention, we will only return true if right = left + 1
    if (llvm::ConstantInt *left_int =
            llvm::dyn_cast<llvm::ConstantInt>(&left)) {
      if (llvm::ConstantInt *right_int =
              llvm::dyn_cast<llvm::ConstantInt>(&right))
        // only works on pairs of integer for now, scev/pattern matching is
        // future work
        return (left_int->getSExtValue() + 1 == right_int->getSExtValue());
    }
    return false;
  }

  void handleIndexReadSeeds(PackSet *ps) {
    for (auto &pair : left_free) {
      if (pair.second.size() <= 0) {
        continue;
      }

      if (!IndexReadInst::classof(*pair.second.begin())) {
        // It's not an IndexReadInst of some kind
        continue;
      }

      auto kind = pair.first;
      auto &left_set = pair.second;
      auto right_iter = right_free.find(kind);
      if (right_iter == left_free.end() || right_iter->second.size() <= 0) {
        // We have right possibilities but no left
        continue;
      }

      auto &right_set = right_iter->second;

      std::unordered_set<MemOIRInst *> left_to_remove;
      for (auto *left_memoir_inst : left_set) {
        IndexReadInst *left_inst =
            static_cast<IndexReadInst *>(left_memoir_inst);
        if (left_inst->getNumberOfDimensions() > 1) {
          // We'll keep this simple for now
          continue;
        }

        llvm::Value &left_index = left_inst->getIndexOfDimension(0);
        std::unordered_set<MemOIRInst *> right_to_remove;
        for (auto *right_memoir_inst : right_set) {
          IndexReadInst *right_inst =
              static_cast<IndexReadInst *>(right_memoir_inst);
          if (right_inst->getNumberOfDimensions()
              != left_inst->getNumberOfDimensions()) {
            continue;
          }

          llvm::Value &right_index = right_inst->getIndexOfDimension(0);

          // check that indexes are adjacent and we are reading from the same
          // sequence
          if (indexesAreAdjacent(left_index, right_index)
              && &left_inst->getObjectOperand()
                     == &right_inst->getObjectOperand()) {
            ps->insertPair(&left_inst->getCallInst(),
                           &right_inst->getCallInst());

            // add for removal to enforce that instructions only occupy one left
            // and right
            left_to_remove.insert(left_memoir_inst);
            right_to_remove.insert(right_memoir_inst);
            break;
          }
        }

        for (auto *right_memoir_inst : right_to_remove) {
          right_set.erase(right_memoir_inst);
        }
      }

      for (auto *left_memoir_inst : left_to_remove) {
        left_set.erase(left_memoir_inst);
      }
    }
  }

  PackSet *createSeededPackSet(void) {
    bool found_match = false;

    std::unordered_set<llvm::memoir::AccessInst *> matched_left;
    std::unordered_set<llvm::memoir::AccessInst *> matched_right;

    PackSet *packset = new PackSet();

    this->handleIndexReadSeeds(packset);

    return packset;
  }
};

struct SLPPass : public llvm::ModulePass {
  static char ID;

  SLPPass() : ModulePass(ID) {}

  bool doInitialization(llvm::Module &M) override {
    return false;
  }

  bool runOnBasicBlock(llvm::BasicBlock &BB) {
    PackSeeder visitor;
    for (llvm::Instruction &i : BB) {
      // llvm::memoir::println(i);
      visitor.visit(i);
    }

    PackSet *packset = visitor.createSeededPackSet();

    llvm::memoir::println("Seeded PackSet: ", packset->dbg_string());

    delete packset;

    return false;
  }

  bool runOnModule(llvm::Module &M) override {
    bool changed;

    for (llvm::Function &F : M) {
      for (llvm::BasicBlock &BB : F) {
        changed |= this->runOnBasicBlock(BB);
      }
    }

    // We did not modify the program, so we return false.
    return changed;
  }

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override {
    return;
  }
};

// Next there is code to register your pass to "opt"
char SLPPass::ID = 0;
static llvm::RegisterPass<SLPPass> X("memoir-vector",
                                     "Trying out SLP Vectorization in MemOIR");
} // namespace
