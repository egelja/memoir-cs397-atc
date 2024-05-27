#pragma once

#include "pack_set.hpp"

#include "llvm/IR/BasicBlock.h"

/**
 * Extend a pack set by following use-def and def-use chains.
 */
PackSet extend_packset(llvm::BasicBlock* BB, PackSet ps);
