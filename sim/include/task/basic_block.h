#ifndef BASIC_BLOCK_H
#define BASIC_BLOCK_H

#include "llvm.h"
#include "value.h"
#include "instruction.h"

#include <string>
#include <vector>
#include <map>

namespace SIM
{
    class BasicBlock : public SIM::Value
    {
    public:
        std::string getBBName() { return bbname; }
        llvm::BasicBlock *getLLVMBasicBlock() { return llvm_bb; }

        BasicBlock(uint64_t id,
                   llvm::BasicBlock *_llvm_bb) : llvm_bb(_llvm_bb), Value(id)
        {
            bbname = llvm_bb->getName().str();
        }
        virtual void initialize(std::unordered_map<llvm::Value *, std::shared_ptr<SIM::Value>> value_map) override;

        std::vector<std::shared_ptr<SIM::Instruction>> getInstructions() { return instructions; }
        std::vector<std::string> getInsnDepData();
        void addInsn(std::shared_ptr<SIM::Instruction> insn) { instructions.push_back(insn); }

    private:
        std::string bbname = "";
        llvm::BasicBlock *llvm_bb = nullptr;
        std::vector<std::shared_ptr<SIM::Instruction>> instructions;
    };
}
#endif