#ifndef INCLUDE_IFU_H
#define INCLUDE_IFU_H

#include <memory>
#include <vector>

#include "task/dyn_insn.h"
#include "mem/mem_transaction.h"
#include "mem/cache.h"
using namespace std;

class Cache;

class IFU
{

public:
    IFU(std::shared_ptr<Cache> _cache) : cache(_cache) {}
    void IF0(std::vector<std::shared_ptr<DynamicOperation>> &FTQ);
    void IF1(std::vector<std::shared_ptr<DynamicOperation>> &processedOps,
             std::vector<std::shared_ptr<DynamicOperation>> &insnQueue);

    int getPackageSize() { return packageSize; }
    void recvResponse(MemTransaction *resp) { responses.push_back(resp); }

private:
    int packageSize = 16;
    bool occupied = false;
    std::vector<MemTransaction *> responses;

    void sendReqToCache(MemTransaction *req);
    std::shared_ptr<Cache> cache;

    std::vector<int> prefetchedAddrs;
    int pfPtr = 0;
};

#endif
