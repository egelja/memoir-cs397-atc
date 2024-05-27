#include "pack.hpp"

#include "memoir/ir/Instructions.hpp"
#include "memoir/support/Print.hpp"

namespace memoir = llvm::memoir;
using llvm::memoir::MemOIRInst;

namespace {

PackType
memoir_inst_type(MemOIRInst* inst)
{
    if (memoir::IndexReadInst::classof(inst))
        return PackType::LOAD;

    if (memoir::IndexWriteInst::classof(inst))
        return PackType::STORE;

    // unknown instruction!
    llvm::memoir::println("Unknown instruction: ", *inst);
    abort();
}

PackType
llvm_inst_type(llvm::Instruction* inst)
{
    switch (inst->getOpcode()) {
        case llvm::Instruction::Add:
            return PackType::ADD;

        case llvm::Instruction::Load:
        case llvm::Instruction::Store:
            llvm::memoir::println("Pack contains a LLVM memop: ", *inst);
            abort();

        default:
            llvm::memoir::println("Unknown instruction: ", *inst);
            abort();
    }
}

} // namespace

size_t
Pack::index_of(llvm::Instruction* inst) const
{
    auto it = std::find(insts_.begin(), insts_.end(), inst);

    if (it == insts_.end())
        return -1;
    return std::distance(insts_.begin(), it);
}

PackType
Pack::type() const
{
    // all instructions should be the same
    auto* inst = insts_[0];
    auto* memoir_inst = MemOIRInst::get(*inst);

    // get based on instruction type
    if (memoir_inst)
        return memoir_inst_type(memoir_inst);

    return llvm_inst_type(inst);
}

std::string
Pack::dbg_string() const
{
    std::string s;
    llvm::raw_string_ostream ss(s);

    ss << "  (\n";

    for (auto* i : insts_)
        ss << "    " << *i << "\n";

    ss << "  )";
    return ss.str();
}

std::string
pack_type_string(PackType type)
{
    switch (type) {
        case PackType::LOAD:
            return "load";

        case PackType::STORE:
            return "store";

        case PackType::ADD:
            return "add";

        default:
            llvm::memoir::println("Invalid pack type!");
            abort();
    }
}
