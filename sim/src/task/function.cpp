#include "task/function.h"
#include "task/basic_block.h"
#include "task/instruction.h"

#include <iostream>

void SIM::Function::initialize(std::unordered_map<llvm::Value *, std::shared_ptr<SIM::Value>> value_map)
{
    for (auto &bb : *(this->llvm_func))
    {
        auto svalue = value_map[&bb];
        std::shared_ptr<SIM::BasicBlock> sbb = std::dynamic_pointer_cast<SIM::BasicBlock>(svalue);
        sbb->initialize(value_map);
        bb_list.push_back(sbb);
    }
}

std::shared_ptr<SIM::BasicBlock> SIM::Function::getEntryBB(std::string target)
{
    for (auto bb : bb_list)
    {
        if (bb->getBBName() == target)
        {
            return bb;
        }
    }

    return nullptr;
}


std::vector<std::string> SIM::Function::getInsnDepData()
{
    std::vector<std::string> ret;
    for (auto bb : bb_list)
    {
        auto temp = bb->getInsnDepData();
        ret.insert(ret.end(), temp.begin(), temp.end());
    }

    return ret;
}