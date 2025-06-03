#include "task/basic_block.h"
#include "task/instruction.h"

#include <iostream>

void SIM::BasicBlock::initialize(std::unordered_map<llvm::Value *, std::shared_ptr<SIM::Value>> value_map)
{
    for (auto &llvm_insn : *(this->llvm_bb))
    {
        auto s_value = value_map[&llvm_insn];
        std::shared_ptr<SIM::Instruction> s_insn = std::dynamic_pointer_cast<SIM::Instruction>(s_value);
        s_insn->initialize(value_map);
        instructions.push_back(s_insn);
    }
}

std::vector<std::string> SIM::BasicBlock::getInsnDepData()
{
    std::vector<std::string> ret;
    for (auto insn : instructions)
    {
        ret.push_back(insn->getInsnDepData());
    }

    return ret;
}