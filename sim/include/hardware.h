#ifndef INCLUDE_HARDWARE_H
#define INCLUDE_HARDWARE_H

#include "task/task.h"
#include "task/dyn_insn.h"
#include "params.h"
#include "statistic.h"
#include "power_area_model.h"

#include <vector>

using namespace std;

struct RetOps
{
public:
    RetOps(std::vector<std::shared_ptr<DynamicOperation>> _ops, bool _isInvoke = false) : ops(_ops), isInvoke(_isInvoke) {}
    std::vector<std::shared_ptr<DynamicOperation>> ops;
    bool isInvoke = false;
};

class Hardware
{
public:
    Hardware() {}
    Hardware(std::shared_ptr<SIM::Task> _task, std::shared_ptr<Params> _params) : task(_task), params(_params) {}
    virtual void initialize() {}
    virtual void finish() {}
    virtual void printStats() {}
    virtual void run() {}

    bool isRunning() { return running; }
    uint64_t getCycle() { return cycles; }
    void setSimpoint(bool _simpoint) { simpoint = _simpoint; }

    double percentage = 1.;

protected:
    std::shared_ptr<SIM::Task> task;
    std::shared_ptr<Params> params;

    string readBBName() { return task->parse_bb_name(); }
    bool readBrCondition() { return task->parse_br(); }
    uint64_t readAddr() { return task->parse_addr(); }
    int readSwitchCondition() { return task->parse_switch(); }
    uint64_t readFuncCycle() { return task->parse_cycle(); }
    std::shared_ptr<SIM::BasicBlock> fetchEntryBB() { return task->getEntry(); }

    std::shared_ptr<SIM::BasicBlock> fetchTargetBB();
    std::shared_ptr<SIM::BasicBlock> fetchTargetBB(std::shared_ptr<BrDynamicOperation> br);
    std::shared_ptr<SIM::BasicBlock> fetchTargetBB(std::shared_ptr<CallDynamicOperation> call);
    std::shared_ptr<SIM::BasicBlock> fetchTargetBB(std::shared_ptr<SwitchDynamicOperation> switchInsn);
    std::shared_ptr<SIM::BasicBlock> fetchTargetBB(std::shared_ptr<InvokeDynamicOperation> invoke);

    std::vector<std::shared_ptr<DynamicOperation>> scheduleBB(std::shared_ptr<SIM::BasicBlock> sbb);

    std::shared_ptr<Statistic> stat;
    PowerAreaModel *pam;

    bool running = true;
    uint64_t cycles = 0;

    bool simpoint = false;
};

#endif