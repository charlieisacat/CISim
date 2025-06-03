#ifndef INCLUDE_WINDOW_H
#define INCLUDE_WINDOW_H
#include "task/dyn_insn.h"
#include <vector>

#include "task/llvm.h"

#include "lsu.h"
#include "statistic.h"

struct ROBEntry
{
public:
    std::shared_ptr<DynamicOperation> op;
    bool empty = true;  //  entry is empty

    ROBEntry(){};

    ROBEntry(
        std::shared_ptr<DynamicOperation> _op,
        bool _empty) : op(_op), empty(_empty) {}
};

class Window
{
public:
    Window(uint64_t _ws, uint64_t _rs, uint64_t _robSize, uint64_t _issueQSize) : windowSize(_ws), retireSize(_rs), robSize(_robSize)
    {
        issueQueueCapacity = _issueQSize;
        issueQueueUsed = 0;
        ROB.assign(robSize, std::make_shared<ROBEntry>());
    }
    std::vector<std::shared_ptr<DynamicOperation>> dispatch(std::vector<std::shared_ptr<DynamicOperation>> &insnQueue);
    bool canDispatch(std::shared_ptr<DynamicOperation> insn);

    void incrementIssueQueueUsed() { issueQueueUsed++; }
    void decrementIssueQueueUsed() { issueQueueUsed--; }
    int getIssueQueueUsed() { return issueQueueUsed; }

    // manipulate rob
    void addInstruction(std::shared_ptr<DynamicOperation> op);
    bool retireInstruction(std::shared_ptr<Statistic> stat, std::shared_ptr<LSUnit> lsu, uint64_t &flushDynID, uint64_t cycle, uint64_t &funcCycle);

    // if all dependent insns finish, set insn ready
    bool checkInsnReady(std::shared_ptr<DynamicOperation> op);
    bool canIssue(std::shared_ptr<DynamicOperation> op);

    int getROBUsed() { return robUsed; }
    bool ifROBFull() { return !ROB[robTail]->empty; }
    bool ifROBEmpty() { return ROB[robHead]->empty; }

    std::vector<std::shared_ptr<DynamicOperation>> flush(uint64_t flushDynID);

    uint64_t getTotalCommit() { return totalCommit; }
    uint64_t getWindowSize() { return windowSize; }
    
    uint64_t rb_flush = 0;
    uint64_t rb_not_retire_slot = 0;

    // dispatch window
    uint64_t windowSize;

    // issue queue
    int issueQueueCapacity;
    int issueQueueUsed;

    // simualtion doesn't need to use seperate queues
    std::vector<std::shared_ptr<DynamicOperation>> issueQueue;

    // rob
    int robSize = 32;
    std::vector<std::shared_ptr<ROBEntry>> ROB;

    // work as RAT
    // map dyn op to rob entry id
    // std::map<std::shared_ptr<DynamicOperation>, int> opEntryMap;
    std::map<uint64_t, int> opEntryMap;

    int robHead = 0;
    int robTail = 0;
    int robUsed = 0;

    uint64_t totalCommit = 0;

    int retireSize = 4;
    uint64_t mispredictNum = 0;
    uint64_t totalPredictNum = 0;
};
#endif