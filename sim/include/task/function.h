#ifndef FUNCTION_H
#define FUNCTION_H

#include "llvm.h"
#include "basic_block.h"
#include "value.h"

#include <string>
#include <vector>
#include <map>

namespace SIM
{
    class Function : public SIM::Value
    {
    public:
        std::string getFuncName() { return funcname; }
        llvm::Function *getLLVMFunction() { return llvm_func; }

        Function(uint64_t id,
                 llvm::Function *_llvm_func) : llvm_func(_llvm_func), Value(id)
        {
            funcname = llvm_func->getName().str();
        }
        virtual void initialize(std::unordered_map<llvm::Value *, std::shared_ptr<SIM::Value>> value_map) override;
        std::shared_ptr<SIM::BasicBlock> getEntryBB() { return bb_list.front(); }
        std::shared_ptr<SIM::BasicBlock> getEntryBB(std::string target);

        std::vector<std::string> getInsnDepData();
        std::vector<std::shared_ptr<SIM::BasicBlock>> getBBList() { return bb_list; }

        void addBB(std::shared_ptr<SIM::BasicBlock> bb) { bb_list.push_back(bb); }

    private:
        std::string funcname = "";
        llvm::Function *llvm_func = nullptr;
        std::vector<std::shared_ptr<SIM::BasicBlock>> bb_list;
    };
}
#endif