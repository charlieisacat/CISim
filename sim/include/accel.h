#ifndef INCLUDE_ACCEL_H
#define INCLUDE_ACCEL_H

#include "hardware.h"
#include "lsu.h"
#include "task/basic_block.h"

#include <unordered_map>
#include <stack>

using namespace std;

// this model is similar to GEM5-SALAM
class Accelerator : public Hardware
{
public:
    Accelerator(shared_ptr<SIM::Task> _task, shared_ptr<Params> _params);
    void tick();
    bool isFinish() { return inflightQueue.size() == 0 && reservation.size() == 0; }
    uint64_t totalCommit = 0;
    uint64_t totalFetch = 0;

    uint64_t temp_acc_cycle = 0;

    uint64_t accumulateCommit = 0;

    virtual void printStats();

    shared_ptr<LSUnit> lsu;
    Hardware *hwHost;

    void stop() { running = false; }
    void switchContext(shared_ptr<SIM::BasicBlock> _targetBB, uint64_t gid);
    void notifyHostFinish();

    void setEntry(shared_ptr<SIM::BasicBlock> entry) { targetBB = entry; }

private:
    list<shared_ptr<DynamicOperation>> reservation;
    unordered_map<uint64_t, shared_ptr<DynamicOperation>> inflightQueue;
    vector<shared_ptr<DynamicOperation>> scoreboard;

    shared_ptr<SIM::BasicBlock> targetBB = nullptr;
    shared_ptr<SIM::BasicBlock> previousBB = nullptr;

    inline bool UIDActive(uint64_t uid) { return inflightQueue.count(uid); }
    // inline bool UIDActive(uint64_t uid) { return inflightQueue.count(uid) || scoreboard.count(uid); }

    void findDynamicDeps(shared_ptr<DynamicOperation> insn);

    // reset this when context switch
    uint64_t globalDynID = 0;

    bool commitInsn(shared_ptr<DynamicOperation> insn);
    void processQueue();

    bool fetch();

    stack<RetOps *> returnStack;
    stack<shared_ptr<SIM::BasicBlock>> prevBBStack;
    bool fetchForRet = false;

    bool offloading = false;
    bool failure = false;

    bool abnormalExit = false;
};

class DyserUnit : public Hardware
{
public:
    DyserUnit(shared_ptr<SIM::Task> _task, shared_ptr<Params> _params, shared_ptr<SIM::BasicBlock> entry);


    void setEntry(shared_ptr<SIM::BasicBlock> entry) { targetBB = entry; }

    void tick();
    void notify(int offset, int stride);
    bool getData(int offset);
    map<int, uint64_t> getInsnCountMap(){return insnCountMap;}

private:

    void fetch();

    uint64_t globalDynID = 0;

    list<shared_ptr<DynamicOperation>> reservation;

    shared_ptr<SIM::BasicBlock> targetBB = nullptr;
    
    map<uint64_t, shared_ptr<DynamicOperation>> uidDynOpMap;
    map<uint64_t, int> outputFIFO;

    map<shared_ptr<DynamicOperation>, map<uint64_t, int>> internalFIFOPerInsn;
    
    map<uint64_t, int> depsCountBackup;
    map<uint64_t, int> depsCount;

    void processQueue();
    unordered_map<uint64_t, shared_ptr<DynamicOperation>> inflightQueue;
    inline bool UIDActive(uint64_t uid) { return inflightQueue.count(uid); }

    vector<uint64_t> checkDataReady(shared_ptr<DynamicOperation> insn);

    map<int, uint64_t> offsetDynIDMap;

    bool commitInsn(shared_ptr<DynamicOperation> insn);

    // uids of insns that use input/output
    map<uint64_t, vector<uint64_t>> inputOffsetUidMap;
    map<uint64_t, vector<uint64_t>> outputOffsetUidMap;

    vector<uint64_t> notifyInputDynIDs;
    
    map<int, uint64_t> insnCountMap;
    void updateInsnCountMap(int opcode); 
};

#endif