#include "task/instruction.h"

void SIM::Instruction::addStaticDependency(std::shared_ptr<SIM::Value> static_node)
{
    static_dependency.push_back(static_node);

    auto insn = std::dynamic_pointer_cast<SIM::Instruction>(static_node);

    llvm::Type *returnType = insn->getLLVMInsn()->getType();
    if (returnType->isFloatingPointTy())
    {
        fpOpNum++;
    }
    else
    {
        intOpNum++;
    }
}

void SIM::Instruction::initialize(std::unordered_map<llvm::Value *, std::shared_ptr<SIM::Value>> value_map)
{
    llvm::User *iruser = llvm::dyn_cast<llvm::User>(llvm_insn);

    for (auto const op : iruser->operand_values())
    {
        // only consider data from instructions
        if (llvm::Instruction *op_insn = llvm::dyn_cast<llvm::Instruction>(op))
        {
            auto mapit = value_map.find(op_insn);
            if (mapit != value_map.end())
            {
                addStaticDependency(mapit->second);
            }
        }
    }
}

std::vector<uint64_t> SIM::Instruction::getStaticDepUIDs()
{
    std::vector<uint64_t> uids;
    for (auto dep : static_dependency)
    {
        uids.push_back(dep->getUID());
    }
    return uids;
}

std::string SIM::Instruction::getInsnDepData()
{
    std::string ret = std::to_string(uid) + ":";

    std::vector<uint64_t> uids = getStaticDepUIDs();
    int size = uids.size();
    for (int i = 0; i < size; i++)
    {
        ret += std::to_string(uids[i]);
        if (i != size - 1)
        {
            ret += ",";
        }
    }

    return ret;
}