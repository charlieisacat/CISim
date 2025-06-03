#ifndef INCLUDE_LSU_H
#define INCLUDE_LSU_H
#include <cstdint>
#include <vector>
#include <memory>
#include <map>

#include "task/llvm.h"
#include "task/dyn_insn.h"
#include "mem/mem_transaction.h"
#include "mem/cache.h"

class Cache;

// load queue entry
struct LDSTQEntry
{
    uint64_t dynID;
    std::shared_ptr<DynamicOperation> op;
    uint64_t address = 0;
    bool empty = true;
    std::vector<std::shared_ptr<DynamicOperation>> forwardList;

    bool dataForwarded = false;
    // which op the load is forwarded from 
    std::shared_ptr<DynamicOperation> forwardOp = nullptr;

    LDSTQEntry() {}
    LDSTQEntry(std::shared_ptr<DynamicOperation> _op, uint64_t _dynID, uint64_t _address, bool _empty) : op(_op), dynID(_dynID), address(_address), empty(_empty) {}
};

struct LDSTQ
{
    int size = 0;
    int usedSize = 0;

    std::vector<std::shared_ptr<LDSTQEntry>> queue;
    std::map<uint64_t, int> entryMap;

    LDSTQ(int _size)
    {
        size = _size;
        queue.assign(size, std::make_shared<LDSTQEntry>());
    }

    int getFreeEntry()
    {
        for (int i = 0; i < queue.size(); i++)
        {
            if (queue[i]->empty)
            {
                return i;
            }
        }
        return -1;
    }

    void addInstruction(std::shared_ptr<DynamicOperation> op)
    {
        assert(op->isLoad() || op->isStore());
        // std::cout << "add entry op type=" << op->isLoad() << " ---" << op->isStore() <<" dynID="<<op->getDynID() << "\n";
        // std::cout << "used size=" << usedSize << "\n";
        int entryID = getFreeEntry();
        assert(entryID != -1);
        queue[entryID] = std::make_shared<LDSTQEntry>(op, op->getDynID(), op->getAddr(), false);

        removeItemByValue(entryMap, entryID);
        entryMap[op->getDynID()] = entryID;

        // op->status = DynOpStatus::ALLOCATED;

        usedSize++;
    }

    // only load will call this
    void completeInstruction(std::shared_ptr<DynamicOperation> op)
    {
        // std::cout << "dynID=" << op->getDynID() << " name=" << op->getOpName() << " isload=" << op->isLoad() << "\n";

        assert(entryMap.find(op->getDynID()) != entryMap.end());
        int entryID = entryMap[op->getDynID()];
        assert(queue[entryID]->op->status == DynOpStatus::EXECUTING ||
               queue[entryID]->op->status == DynOpStatus::SLEEP);
        // update status in writeback
        op->setFinish(true);
    }

    // store can be fire now
    void commitInstruction(std::shared_ptr<DynamicOperation> op)
    {
        assert(entryMap.find(op->getDynID()) != entryMap.end());
        int entryID = entryMap[op->getDynID()];
        // queue[entryID]->status = DynOpStatus::COMMITED;
        assert(queue[entryID]->op->status == DynOpStatus::FINISHED);
        queue[entryID]->op->updateStatus();
        // std::cout << "freeentry opid=" << op->getDynID() << " name=" << op->getOpName() << "\n";
    }

    // TODO:
    // free entry when insn commit in rob ?
    // mem req should fire in this cycle or next cycle ?
    void freeEntry(std::shared_ptr<DynamicOperation> op)
    {
        // std::cout << "free entry op type=" << op->isLoad() << " ---" << op->isStore() <<" dynID="<<op->getDynID() << "\n";
        assert(entryMap.find(op->getDynID()) != entryMap.end());
        int entryID = entryMap[op->getDynID()];
        queue[entryID] = std::make_shared<LDSTQEntry>();
        usedSize--;
        assert(usedSize >= 0);
    }

    bool ifFull()
    {
        // if (usedSize >= size)
        // {
        //     for (int i = 0; i < queue.size(); i++)
        //     {
        //         if (!queue[i]->empty)
        //         {
        //             std::cout << "entry in stq:" << queue[i]->op->getDynID() << "\n";
        //         }
        //     }
        // }

        return usedSize >= size;
    }

    void flush()
    {
        for (int i = 0; i < queue.size(); i++)
        {
            if (!queue[i]->empty && queue[i]->op->isSquashed)
            {
                queue[i] = std::make_shared<LDSTQEntry>();
                usedSize--;
                assert(usedSize >= 0);
            }
        }
    }

    int getUsedSize()
    {
        return usedSize;
    }
};

class LSUnit
{
public:
    LSUnit(std::shared_ptr<Cache> _cache, int ldqSize, int stqSize) : cache(_cache)
    {
        LDQ = std::make_shared<LDSTQ>(ldqSize);
        STQ = std::make_shared<LDSTQ>(stqSize);
    }

    // allocate entry in ld/st queue
    void addInstruction(std::shared_ptr<DynamicOperation> op);
    void commitInstruction(std::shared_ptr<DynamicOperation> op);
    void process();
    void wb();

    int getExecutingLoads();

    // flush because of
    // 1. memory disambiguation
    // 2. load-load ordering in multi-core scenario (for data cohenrency)
    bool shouldFlush(std::shared_ptr<DynamicOperation> op);
    bool ifLDQFull() { return LDQ->ifFull(); }
    bool ifSTQFull() { return STQ->ifFull(); }
    void flush();
    
    int getLDQUsedSize() { return LDQ->getUsedSize(); }
    int getSTQUsedSize() { return STQ->getUsedSize(); }

    // Mem Transaction
    void recvResponse(MemTransaction *resp) { responses.push_back(resp); }
    void sendReqToCache(MemTransaction *req);

    // can allocate entry
    bool canDispatch(std::shared_ptr<DynamicOperation> op, bool cpu = true);

    void issue(std::shared_ptr<DynamicOperation> op);

    std::shared_ptr<LDSTQEntry> forwardData(std::shared_ptr<DynamicOperation> op);
    std::shared_ptr<LDSTQEntry> getAliasOp(std::shared_ptr<DynamicOperation> op);

    void doForwardData(std::shared_ptr<LDSTQEntry> entry);

    int lsu_port = 4;
    int used_port = 0;

private:
    std::shared_ptr<LDSTQ> LDQ;
    std::shared_ptr<LDSTQ> STQ;

    // Mem Transaction
    std::vector<MemTransaction *> responses;

    // L1Cache
    std::shared_ptr<Cache> cache = nullptr;
    // cache->recvRequests();
};

#endif