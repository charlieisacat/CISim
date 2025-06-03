#ifndef TASK_H
#define TASK_H

#include "basic_block.h"
#include "function.h"
#include <vector>
#include <fstream>

namespace SIM
{
    class Task
    {
    public:
        Task(std::shared_ptr<SIM::BasicBlock> _entry,
             std::unordered_map<llvm::Value *, std::shared_ptr<SIM::Value>> _vmap,
             std::string _trace_file) : entry(_entry), vmap(_vmap), trace_file(_trace_file) {}

        std::shared_ptr<SIM::BasicBlock> getEntry() { return entry; }
        std::shared_ptr<SIM::BasicBlock> fetchBB(llvm::Value *llvmValue);
        std::shared_ptr<SIM::Function> fetchFunc(llvm::Value *llvmValue);

        void initialize();
        void finish();

        // parse trace file to get cond and addr
        bool parse_br();
        int parse_switch();
        uint64_t parse_addr();
        uint64_t parse_cycle();
        std::string parse_bb_name();

        bool at_end() { return trace_fs.eof(); }

        std::unordered_map<std::string, llvm::Value *> name_bb_map;
        std::unordered_map<llvm::Value *, std::shared_ptr<SIM::Value>> getValueMap() { return vmap; }

        void saveRollbackPos() { rollbackPos = trace_fs.tellg(); }
        void resetReadingPos() { trace_fs.seekg(rollbackPos); }

    private:
        std::shared_ptr<SIM::BasicBlock> entry;
        std::unordered_map<llvm::Value *, std::shared_ptr<SIM::Value>> vmap;
        std::string trace_file;
        std::fstream trace_fs;

        std::streampos rollbackPos;
    };
}

#endif