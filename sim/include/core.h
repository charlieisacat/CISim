#ifndef INCLUDE_CORE_H
#define INCLUDE_CORE_H

#include "hardware.h"
#include "task/task.h"
#include "task/dyn_insn.h"
#include <stack>
#include "window.h"
#include "params.h"
#include "function_unit.h"
#include "port.h"
#include "issue_unit.h"
#include "lsu.h"
#include "mem/cache.h"
#include "mem/DRAM.hpp"
#include "branch_predictor.h"
#include "statistic.h"
#include "power_area_model.h"

#include "accel.h"
#include "rou.h"
#include "ifu.h"

#include <vector>

class Core : public Hardware
{
public:
    Core(std::shared_ptr<SIM::Task> _task, std::shared_ptr<Params> _params);

    virtual void run() override;

    /********************************* */
    // some statistics
    uint64_t flushN = 0;
    uint64_t funcCycles = 0;
    uint64_t flushedInsnNumber = 0;
    uint64_t rb_flushedN = 0;
    uint64_t stalls_mem_any = 0;

    uint64_t temp_acc_cycle = 0;

    // just for check correctness
    uint64_t totalFetch = 0;
    uint64_t totalExe = 0;

    uint64_t globalDynID = 0;

    bool flushStall = false;
    int flushStallCycle = 0;
    int flushPenalty = 0;

    uint64_t totalPredictNum = 0;

    // backend bound cycles
    uint64_t stalls_total = 0;
    uint64_t cycles_ge_1_uop_exec = 0;
    uint64_t cycles_ge_2_uop_exec = 0;
    uint64_t cycles_ge_3_uop_exec = 0;

    uint64_t rs_empty_cycles = 0;
    uint64_t stall_sb = 0;

    /********************************* */

    virtual void initialize() override;
    virtual void finish() override;
    virtual void printStats() override;
    void printForMcpat();

    std::shared_ptr<L1Cache> getL1Cache() { return l1c; };
    std::shared_ptr<L2Cache> getL2Cache() { return l2c; };
    std::shared_ptr<Window> getWindow() { return window; }

    void switchContext(uint64_t gid, bool failure = false, bool abnormal = false);
    uint64_t getTotalCommit();

    // void setDyserEntry(shared_ptr<SIM::BasicBlock> entry);
    void setDyserEntry(unordered_map<int, shared_ptr<SIM::Function>> dyser_func_map);

    std::shared_ptr<Accelerator> getAccelerator() { return acc; }

private:
    uint64_t skip_insns_n = 0;

    // L1D Cache
    std::shared_ptr<L1Cache> l1c;
    // L1i Cache
    std::shared_ptr<L1Cache> l1i;
    // L2 Cache
    std::shared_ptr<L2Cache> l2c;

    std::shared_ptr<Window> window;

    // for BP 
    std::stack<std::shared_ptr<DynamicOperation>> prevBBTermStack;
    std::shared_ptr<DynamicOperation> prevBBTerminator;

    // FTP only stores starting insn of a basic block
    std::vector<std::shared_ptr<DynamicOperation>> FTQ;
    std::vector<std::shared_ptr<DynamicOperation>> insnQueue;
    std::vector<std::shared_ptr<DynamicOperation>> processedOps;
    uint64_t insnQueueMaxSize = 1024;

    // stack for return address
    std::stack<RetOps *> returnStack;
    // corresponding previous BB of return
    std::stack<std::shared_ptr<SIM::BasicBlock>> prevBBStack;

    // fetch
    std::shared_ptr<SIM::BasicBlock> previousBB = nullptr;
    std::shared_ptr<SIM::BasicBlock> targetBB = nullptr;
    std::vector<std::shared_ptr<DynamicOperation>> fetchOpBuffer;
    bool fetchForRet = false;
    void fetch();

    std::vector<std::shared_ptr<DynamicOperation>> preprocess();

    // dispatch
    void dispatch();
    void insnToIssueQueues(std::vector<std::shared_ptr<DynamicOperation>> &insnToIssue);
    std::vector<std::shared_ptr<DynamicOperation>> issueQueue;
    // insn in fu pipelines
    std::vector<std::shared_ptr<DynamicOperation>> inFlightInsn;

    std::map<uint64_t, std::shared_ptr<DynamicOperation>> uidDynOpMap;

    // issue
    void issue();
    std::vector<std::shared_ptr<Port>> ports;
    std::shared_ptr<LSUnit> lsu;

    // execute
    void execute();

    // issue and execute
    void IE();

    // write back
    void writeBack();

    // commit
    std::vector<uint64_t> commitInsns;
    void commit();

    // flush
    std::vector<std::shared_ptr<DynamicOperation>> flushedInsns;

    // issue unit
    std::shared_ptr<IssueUnit> issueUnit;

    std::shared_ptr<Bpred> bp;

    // acc
    std::string offload_func_name = "";
    std::shared_ptr<Accelerator> acc;
    void offload(std::shared_ptr<Accelerator> acc, std::shared_ptr<SIM::BasicBlock> targetBB, uint64_t gid);
    bool offloading = false;

    // is speculative accel
    bool speculative = false;
    bool rollback = false;
    bool switching = false;

    bool abnormalExit = false;

    bool dyser = false;
    // std::shared_ptr<DyserUnit> dyserUnit;
    std::shared_ptr<SIM::BasicBlock> dyserEntryBB = nullptr;

    unordered_map<int, shared_ptr<DyserUnit>> dyserUnits;

    uint64_t offloadNumber = 0;
    uint64_t rollbackNumber = 0;

    std::shared_ptr<RequestOrderingUnit> rou;

    std::shared_ptr<IFU> ifu;
    
};

#endif