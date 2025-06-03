#ifndef INCLUDE_STATISTIC_H
#define INCLUDE_STATISTIC_H

#include <map>
#include "task/dyn_insn.h"
#include "task/llvm.h"

class Statistic
{
public:
    Statistic() {}

    // opcode and dynamic count map
    std::map<llvm::Instruction *, uint64_t> insnCountMap;
    
    std::map<uint32_t, uint64_t> customInsnCountMap;
    std::map<uint32_t, double> customInsnAreaMap;
    std::map<uint32_t, double> customInsnPowerMap;

    uint64_t totalWrite = 0;

    void updateInsnCount(std::shared_ptr<DynamicOperation> op);
    void updateCustomInsnCount(std::shared_ptr<DynamicOperation> op);
    void updateCustomInsnArea(std::shared_ptr<DynamicOperation> op, double area);
    void updateCustomInsnPower(std::shared_ptr<DynamicOperation> op, double power);

    bool shouldUpdateArea(std::shared_ptr<DynamicOperation> op);
    bool shouldUpdatePower(std::shared_ptr<DynamicOperation> op);

    uint64_t packing_func_num = 0;
    uint64_t extract_insn_num = 0;

    uint64_t packing_insn_stall = 0;
    uint64_t ext_insn_stall = 0;
    uint64_t cstm_insn_stall = 0;

    uint64_t intRFReads = 0;
    uint64_t intRFWrites = 0;
    uint64_t fpRFReads = 0;
    uint64_t fpRFWrites = 0;
};

#endif