#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include "llvm.h"
#include "value.h"

#include <string>
#include <map>

namespace SIM
{
    // static instruction
    class Instruction : public SIM::Value
    {
    public:
        std::string getOpName() { return opname; }
        std::string getIRString() { return ir_string; }
        uint32_t getOpCode() { return opcode; }
        llvm::Instruction *getLLVMInsn() { return llvm_insn; }

        Instruction(uint64_t id,
                    llvm::Instruction *_llvm_insn) : llvm_insn(_llvm_insn), Value(id)
        {

            opname = llvm_insn->getOpcodeName();
            opcode = llvm_insn->getOpcode();
            llvm::raw_string_ostream rso(ir_string);
            llvm_insn->print(rso);
        }

        void addStaticDependency(std::shared_ptr<SIM::Value> static_node);

        virtual void initialize(std::unordered_map<llvm::Value *, std::shared_ptr<SIM::Value>> value_map) override;
        std::vector<uint64_t> getStaticDepUIDs();
        std::vector<std::shared_ptr<SIM::Value>> getStaticDependency() { return static_dependency; }
        std::string getInsnDepData();

        int intOpNum = 0;
        int fpOpNum = 0;

    private:
        std::string ir_string = "";
        std::string opname = "";
        uint32_t opcode = 0;
        llvm::Instruction *llvm_insn = nullptr;

        // static dependency may be insn and bb
        std::vector<std::shared_ptr<SIM::Value>> static_dependency;
    };

}
#endif
