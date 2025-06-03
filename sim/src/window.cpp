#include "window.h"
#include <iostream>
#include "task/llvm.h"

std::vector<std::shared_ptr<DynamicOperation>> Window::flush(uint64_t flushDynID)
{

    std::vector<std::shared_ptr<DynamicOperation>> flushedInsns;
    for (auto robEntry : ROB)
    {
        // ops of dynID < flushDynID may issed later than flushDynID
        if (!robEntry->empty && robEntry->op->getDynID() > flushDynID)
        {
            auto clone = robEntry->op->clone();
            clone->reset();
            flushedInsns.push_back(clone);

            robEntry->op->isSquashed = true;

            // std::cout << "rob flush " << robEntry->op->getDynID() << " name=" << robEntry->op->getOpName()
            //           << " ==" << flushDynID << " squashed=" << robEntry->op->isSquashed << "\n";
        }
    }

    ROB.clear();
    ROB.assign(robSize, std::make_shared<ROBEntry>());
    opEntryMap.clear();
    robHead = 0;
    robTail = 0;
    robUsed = 0;
    issueQueueUsed = 0;

    return flushedInsns;
}

bool Window::canDispatch(std::shared_ptr<DynamicOperation> insn)
{
    return issueQueueUsed < issueQueueCapacity;
}

std::vector<std::shared_ptr<DynamicOperation>> Window::dispatch(std::vector<std::shared_ptr<DynamicOperation>> &insnQueue)
{
    std::vector<std::shared_ptr<DynamicOperation>> insnToIssue;
    for (int i = 0; i < windowSize; i++)
    {
        // check if free list is full here
        if (insnQueue.size() && canDispatch(insnQueue[0]) && !ifROBFull())
        {
            // if (insnQueue[0]->getDynID() == 22)
            // {
            //     std::cout << "dispatch " << insnQueue[0]->getDynID() << " name=" << insnQueue[0]->getOpName() << "\n";
            // }
            // to issue queue
            insnToIssue.push_back(insnQueue[0]);
            incrementIssueQueueUsed();

            // to rob
            addInstruction(insnQueue[0]);

            insnQueue.erase(insnQueue.begin());
        }
        else
        {
            break;
        }
    }
    return insnToIssue;
}

void Window::addInstruction(std::shared_ptr<DynamicOperation> op)
{
    // for (auto item : opEntryMap)
    // {
    //     std::cout << "item.first=" << item.first << " second=" << item.second << "\n";
    // }
    // std::cout << "into rob=" << op->getDynID() << " type=" << op->getOpName() << " robtail=" << robTail << "\n";
    assert(opEntryMap.find(op->getDynID()) == opEntryMap.end());

    // ready is true, so insn can directly run
    // without checking data depedency
    ROB[robTail] = std::make_shared<ROBEntry>(op, false);
    // std::cout << "robTail=" << robTail << " addr=" << op << "\n";
    // opEntryMap.insert({op, robTail});
    // opEntryMap[op] = robTail;
    removeItemByValue(opEntryMap, robTail);
    opEntryMap[op->getDynID()] = robTail;

    robTail = (robTail + 1) % robSize;

    robUsed++;
}

bool Window::retireInstruction(std::shared_ptr<Statistic> stat, std::shared_ptr<LSUnit> lsu, uint64_t &flushDynID, uint64_t cycle, uint64_t &funcCycle)
{
    bool flush = false;
    bool memOrdering = false;
    // bool mispredict = false;
    int n = 0;

    // retire until insn does not complete
    while (ROB[robHead]->empty == false && ROB[robHead]->op->status == DynOpStatus::FINISHED)
    {
        auto op = ROB[robHead]->op;
        if (op->isStore())
            memOrdering = lsu->shouldFlush(op);

        if (op->isBr())
        {
            totalPredictNum++;
            if (op->mispredict)
            {
                // mispredict = true;
                mispredictNum++;
            }
        }

        // flush = mispredict | memOrdering;

        assert(op->status == DynOpStatus::FINISHED);
        op->R = cycle;

        // auto op = ROB[robHead]->op;
        ROB[robHead]->empty = true;
        // ROB[robHead] = std::make_shared<ROBEntry>();
        robHead = (robHead + 1) % robSize;
        robUsed--;
        totalCommit++;
        stat->updateInsnCount(op);

        // std::cout << "retire rob=" << op->getDynID() << " type=" << op->getOpName() << " robhead=" << robHead << " totalCommit=" << totalCommit << " addr=" << op->getAddr() << "\n";

        // free entry in lsu queues, which works if op is load/store
        // lsu->freeEntry(op);
        if (op->isCall())
        {
            funcCycle += op->cycle;
        }

        if (op->isLoad() || op->isStore())
            lsu->commitInstruction(op);
        else
            op->updateStatus();
        n++;

        // std::cout << "sID=" << op->getUID() << " dynID=" << op->getDynID() << " opname=" << op->getOpName() << " D=" << op->D << " e=" << op->e << " E=" << op->E << " R=" << op->R << "\n";
        // std::cout << "    deps:";

        // auto deps = op->getDynDeps();
        // for (auto dep : deps)
        // {
        //     std::cout << dep << " ";
        // }
        // std::cout << "\n";

        rb_not_retire_slot += (retireSize - n);

        if (flush)
        {
            if (memOrdering)
                rb_flush++;
            // if (mispredict)
            //     mispredictNum++;

            // std::cout << "flush op=" << op->getDynID() << " type=" << op->getOpName() << " robhead=" << robHead << "addr " << op->getAddr() << "\n";
            flushDynID = op->getDynID();
            // std::cout << "flush " << " head=" << robHead << " empty=" << ROB[robHead]->empty << " head" << ROB[robHead]->op->getDynID() << " flushid=" << flushDynID << "\n";
            break;
        }

        if (n >= retireSize)
            break;
    }

    return flush;
}

bool Window::canIssue(std::shared_ptr<DynamicOperation> op)
{
    assert(opEntryMap.find(op->getDynID()) != opEntryMap.end());
    // if (op->status == DynOpStatus::READY)
    //     return true;
    return checkInsnReady(op);

    // auto entryID = opEntryMap[op->getDynID()];
    // return ROB[entryID]->ready;
}

// unique id for dyn op
bool Window::checkInsnReady(std::shared_ptr<DynamicOperation> op)
{
    auto entryID = opEntryMap[op->getDynID()];

    auto deps = op->getDynDeps();
    bool ready = true;
    // std::cout << "ready dynid" << op->getDynID() << " opname=" << op->getOpName() << " deps size=" << deps.size() << "\n";
    // for (auto item : opEntryMap)
    // {
    //     std::cout << "item1=" << item.first << " item2=" << item.second << "\n";
    // }

    vector<uint64_t> readRFUids;
    for (auto dep : deps)
    {
        // if not in rob, we consider them in regs
        if (opEntryMap.find(dep) != opEntryMap.end())
        {
            // std::cout << " entry id=" << opEntryMap[dep] << "\n";
            auto entry = ROB[opEntryMap[dep]];
            if (!entry->empty)
            {
                // std::cout << "  >>> dep=" << dep << " ready=" << (entry->op->status == DynOpStatus::FINISHED) << "\n";
                ready &= (entry->op->status == DynOpStatus::FINISHED);
            }
        }
        else
        {
            readRFUids.push_back(dep);
        }
    }

    // rf reads
    if (ready)
    {
        op->readRFUids = readRFUids;
    }

    // std::cout << "  ---ready=" << ready << "\n";
    // ROB[entryID]->ready = ready;
    return ready;
}
