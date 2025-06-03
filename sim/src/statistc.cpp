#include "statistic.h"

void Statistic::updateInsnCount(std::shared_ptr<DynamicOperation> op)
{
    llvm::Instruction *insn = op->getStaticInsn()->getLLVMInsn();

    if (insnCountMap.find(insn) == insnCountMap.end())
        insnCountMap.insert(std::make_pair(insn, 0));

    insnCountMap[insn]++;

    if (!insn->getType()->isVoidTy())
        totalWrite++;

    llvm::Type *returnType = insn->getType();
    if (returnType->isFloatingPointTy())
    {
        fpRFWrites++;
        // if (op->getOpName() == "load" || op->getOpName() == "store" || op->getOpName() == "sitofp" || op->getOpName() == "uitofp")
        // {
        //     intRFReads += op->readRFUids.size();
        // }
        // else
        // {
        //     fpRFReads += op->readRFUids.size();
        // }
    }
    else
    {
        intRFWrites++;
        // if (op->getOpName() == "fptosi" || op->getOpName() == "fptoui")
        // {
        //     fpRFReads += op->readRFUids.size();
        // }
        // else
        // {
        //     intRFReads += op->readRFUids.size();
        // }
    }
        
        fpRFReads += op->getStaticInsn()->fpOpNum;
        intRFReads += op->getStaticInsn()->intOpNum;
}

void Statistic::updateCustomInsnCount(std::shared_ptr<DynamicOperation> op)
{
    uint32_t opcode = op->opcode;

    if (customInsnCountMap.find(opcode) == customInsnCountMap.end())
        customInsnCountMap.insert(std::make_pair(opcode, 0));

    customInsnCountMap[opcode]++;
}

void Statistic::updateCustomInsnArea(std::shared_ptr<DynamicOperation> op, double area)
{
    uint32_t opcode = op->opcode;

    if (customInsnAreaMap.find(opcode) == customInsnAreaMap.end())
        customInsnAreaMap.insert(std::make_pair(opcode, area));
}

void Statistic::updateCustomInsnPower(std::shared_ptr<DynamicOperation> op, double power)
{
    uint32_t opcode = op->opcode;

    if (customInsnPowerMap.find(opcode) == customInsnPowerMap.end())
        customInsnPowerMap.insert(std::make_pair(opcode, power));
}

bool Statistic::shouldUpdateArea(std::shared_ptr<DynamicOperation> op)
{
    uint32_t opcode = op->opcode;
    if (customInsnAreaMap.find(opcode) == customInsnAreaMap.end())
        return true;

    return false;
}

bool Statistic::shouldUpdatePower(std::shared_ptr<DynamicOperation> op)
{
    uint32_t opcode = op->opcode;
    if (customInsnPowerMap.find(opcode) == customInsnPowerMap.end())
        return true;

    return false;
}