#ifndef CACHE_H
#define CACHE_H
#include "mem_transaction.h"
#include "lsu.h"
#include "ifu.h"
#include "FunctionalCache.hpp"
#include "DRAM.hpp"

#include <vector>
#include <memory>
#include <queue>
#include <set>
#include <unordered_map>

class LSUnit;
class IFU;
class DRAMSimInterface;

class MSHREntry
{
public:
    /** \brief   */
    std::vector<MemTransaction *> opset;
};

class Cache
{
public:
    Cache() {}
    Cache(int _latency,
          int _cacheLineSize,
          int _cacheSize,
          int _assoc, int _mshr,
          int _store_ports, int _load_ports) : latency(_latency),
                                               cacheLineSize(_cacheLineSize),
                                               cacheSize(_cacheSize),
                                               assoc(_assoc), store_ports(_store_ports), load_ports(_load_ports)
    {
        if (_mshr == 0)
        {
            useMSHR = false;
        }
        else
        {
            useMSHR = true;
            mshrSize = _mshr;
        }

        free_load_ports = load_ports;
        free_store_ports = store_ports;
    }

    // recv requests from lower cache/cpu
    void recvRequest(MemTransaction *req)
    {
        incomingRequests.push_back(req);

        if (req->isLoad)
        {
            free_load_ports--;
            assert(free_load_ports >= 0);
        }
        else if (req->isPrefetch)
        {
            free_prefetch_ports--;
            assert(free_prefetch_ports >= 0);
        }
        else
        {
            free_store_ports--;
            assert(free_store_ports >= 0);
        }
    }
    // recv responses from higher cache/dram
    void recvResponse(MemTransaction *resp) { incomingResponses.push_back(resp); }

    virtual void process();

    void addTransaction(MemTransaction *t, uint64_t extra_lat);
    void flush();

    // the two should be implemented based on which cache-level they are
    virtual void sendResponses() { assert(0); }
    virtual void sendRequests() { assert(0); };

    virtual void handleRequests();
    virtual void handleResponses();

    void completeTransaction(MemTransaction *t);
    int getCacheLineSize() { return cacheLineSize; }

    std::shared_ptr<LSUnit> lsu = nullptr;
    std::shared_ptr<IFU> ifu = nullptr;

    std::shared_ptr<Cache> nextCache = nullptr;

    std::shared_ptr<Cache> prevCache = nullptr;

    std::shared_ptr<Cache> prevDCache = nullptr;
    std::shared_ptr<Cache> prevICache = nullptr;

    DRAMSimInterface *memInterface = nullptr;

    // return true if CL in previous cache is dirty
    bool invalidate(uint64_t address);
    void updateMSHR(MemTransaction *t);

    // if this cache and previous cache evict CL in the same cycle, handle it here
    // return true if conflict,
    // we will abort eviction from previcous level,
    // and set CL in this level to dirty and send it to next level
    bool checkEvictConflict(uint64_t address);

    bool useMSHR = false;
    int mshrSize = 8;

    int getMshrUsedSize() { return mshr.size(); }
    // return true if MSHR is full
    bool enterMSHR(MemTransaction *t);

    double getHitRate() { return nhit * 1.0 / ((nmiss + nhit) * 1.0); }
    double getMissRate() { return nmiss * 1.0 / ((nmiss + nhit) * 1.0); }

    int level = 0;
    int nhit = 0;
    int nmiss = 0;

    bool willAcceptTransaction(MemTransaction *t);
    uint64_t portStall = 0;

    uint64_t read_accesses = 0;
    uint64_t write_accesses = 0;
    uint64_t read_misses = 0;
    uint64_t write_misses = 0;

protected:
    std::vector<MemTransaction *> incomingRequests;
    std::vector<MemTransaction *> incomingResponses;
    std::vector<MemTransaction *> outcomingRequests;
    std::vector<MemTransaction *> outcomingResponses;

    std::vector<MemTransaction *> stalledTransactions;

    std::priority_queue<TransactionOp, std::vector<TransactionOp>, TransactionOpCompare> pq;

    // access delay
    int latency = 0;

    // global cycles
    uint64_t cycle = 0;

    // functional cache
    std::shared_ptr<FunctionalCache> fc = nullptr;
    int cacheLineSize = 64;
    int cacheSize = 32678;
    int assoc = 8;

    // <evict address, isdirty>
    std::vector<std::pair<uint64_t, bool>> evictAddresses;

    std::unordered_map<uint64_t, MSHREntry> mshr;

    /** \brief  Number of load ports.**/
    int load_ports = 4;

    /** \brief  Number of store ports.
        The maxmium number of store transaction.
    */
    int store_ports = 4;
    /** \brief  Current free load ports */
    int free_load_ports = 4;
    /** \brief Current free store ports  */
    int free_store_ports = 4;

    /** \brief Memory transaction to prefetch  */
    vector<MemTransaction *> to_prefetch;
    /** \brief Number of bytes to be prefeteched */
    int prefetch_set_size = 128;
    /** \brief order of loads. Not vert clear. The MSHR mechanisim needs to be reviced.   */
    queue<uint64_t> prefetch_queue;
    /** \brief Not vert clear. The MSHR mechanisim needs to be reviced. */
    unordered_set<uint64_t> prefetch_set;
    /** \brief how many neighboring addresses to check to determine spatially local accesses */
    int pattern_threshold = 4;
    /** \brief bytes of strided access */
    int min_stride = 4;
    /** \brief Number of ports dedicated to the prefetch */
    int cache_prefetch_ports = 4;
    /** \brief Live nb of ports dedicated to the prefetch */
    int free_prefetch_ports;
    /** \brief   */
    int prefetch_distance = 1;
    /** \brief   degree*/
    int num_prefetched_lines = 8;

    map<int, pair<uint64_t, int>> prefetch_map;

    void addPrefetch(MemTransaction *t, uint64_t extra_lat);
};

class L1Cache : public Cache
{
public:
    L1Cache(int _latency, int _cacheLineSize, int _cacheSize,
            int _assoc, int _mshr, int _store_ports, int _load_ports, bool _isICache = false) : Cache(_latency, _cacheLineSize, _cacheSize, _assoc,
                                                                                                      _mshr, _store_ports, _load_ports)
    {
        level = 1;
        fc = std::make_shared<FunctionalCache>(cacheSize, assoc, cacheLineSize);
        isICache = _isICache;
    }
    virtual void process() override;

    // send responses to cpu
    virtual void sendResponses() override;

    // send requests to dram
    virtual void sendRequests() override;

    virtual void handleRequests() override;

    bool isICache = true;
};

class L2Cache : public Cache
{
public:
    L2Cache(int _latency, int _cacheLineSize, int _cacheSize, int _assoc, int _mshr, int _store_ports, int _load_ports) : Cache(_latency, _cacheLineSize, _cacheSize, _assoc, _mshr, _store_ports, _load_ports)
    {
        level = 2;
        fc = std::make_shared<FunctionalCache>(cacheSize, assoc, cacheLineSize);
    }
    virtual void process() override;

    // send responses to cpu
    virtual void sendResponses() override;

    // send requests to dram
    virtual void sendRequests() override;
};

class LLCache : public Cache
{
public:
    LLCache(int _latency, int _cacheLineSize, int _cacheSize, int _assoc, int _mshr, int _store_ports, int _load_ports) : Cache(_latency, _cacheLineSize, _cacheSize, _assoc, _mshr, _store_ports, _load_ports)
    {
        level = 3;
        fc = std::make_shared<FunctionalCache>(cacheSize, assoc, cacheLineSize);
    }
    virtual void process() override;

    // send responses to cpu
    virtual void sendResponses() override;

    // send requests to dram
    virtual void sendRequests() override;
};

#endif
