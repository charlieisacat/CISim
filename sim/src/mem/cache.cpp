#include "mem/cache.h"

// enter pq
// at specific cycle, execute
// if hit, respond to cpu
// else send requests
// when recv responses from dram, respond to cpu

// mshr
// if hit, responde
// if miss:
// check transaction is in mshr if true do nothing because this cacheline is already miss
// if false insert into mshr, and request
// receive a response
// update mshr and reponde to lsu/prev caches

void L1Cache::process()
{
    sendResponses();
    sendRequests();

    // put it here before process
    handleRequests();
    // process pq

    for (auto t : to_prefetch)
        if (free_prefetch_ports > 0)
            addPrefetch(t, -1);
        else
            break;

    Cache::process();
    handleResponses();

    cycle++;
    free_load_ports = load_ports;
    free_store_ports = store_ports;
    free_prefetch_ports = cache_prefetch_ports;
}

void L1Cache::sendRequests()
{
    // requests because of miss
    for (auto it = outcomingRequests.begin(); it != outcomingRequests.end();)
    {
        MemTransaction *t = *it;

        // outcomingRequests in L1Cache don't have any evict transactions
        if (nextCache->willAcceptTransaction(t))
        {
            nextCache->recvRequest(t);
            it = outcomingRequests.erase(it);
        }
        else
        {
            portStall++;
            it++;
        }
    }

    // evict
    for (auto it = evictAddresses.begin(); it != evictAddresses.end();)
    {
        uint64_t eAddr = it->first;
        bool dirty = it->second;
        // std::cout << "l1 invalid=" << "cycle=" << cycle << " eAddr= " << eAddr << " tag=" << fc->getTag(eAddr) << " setid=" << fc->getSetid(eAddr) << " dity=" << dirty << "\n";
        MemTransaction *t = new MemTransaction(-1, -1, nullptr, eAddr, false);

        // if (dirty)
        // {
        //     nextCache->recvRequest(t);
        // }

        if (dirty)
        {
            if (nextCache->willAcceptTransaction(t))
            {
                nextCache->recvRequest(t);
            }
            else
            {
                outcomingRequests.push_back(t);
            }
        }
        it = evictAddresses.erase(it);
    }
}

void L1Cache::sendResponses()
{
    // std::cout << "oucomingResp.size()=" << outcomingResponses.size() << "\n";
    for (auto resp : outcomingResponses)
    {
        // filter prefetch transactions
        if (!resp->isPrefetch)
        {
            // store reqs dont need to response
            if (resp->isLoad && !resp->fromICache)
                lsu->recvResponse(resp);

            if (resp->isLoad && resp->fromICache)
                ifu->recvResponse(resp);
        }
    }
    outcomingResponses.clear();
}

void L2Cache::process()
{
    sendResponses();
    sendRequests();

    handleRequests();
    // process pq
    Cache::process();
    handleResponses();

    cycle++;
    free_load_ports = load_ports;
    free_store_ports = store_ports;
}

void L2Cache::sendRequests()
{
    for (auto it = outcomingRequests.begin(); it != outcomingRequests.end();)
    {
        MemTransaction *t = *it;

        if (nextCache->willAcceptTransaction(t))
        {
            nextCache->recvRequest(t);
            it = outcomingRequests.erase(it);
        }
        else
        {
            portStall++;
            it++;
        }
    }

    for (auto it = evictAddresses.begin(); it != evictAddresses.end();)
    {
        uint64_t eAddr = it->first;
        bool dirty = it->second;
        MemTransaction *t = new MemTransaction(-1, -1, nullptr, eAddr, false);
        // std::cout << "l2 invalid=" << "cycle=" << cycle << " eAddr= " << eAddr << " tag=" << fc->getTag(eAddr) << " setid=" << fc->getSetid(eAddr) << " dity=" << dirty << "\n";
        dirty |= prevICache->invalidate(eAddr);
        dirty |= prevDCache->invalidate(eAddr);
        dirty |= checkEvictConflict(eAddr);

        // if (dirty)
        // {
        //     nextCache->recvRequest(t);
        // }

        if (dirty)
        {
            if (nextCache->willAcceptTransaction(t))
            {
                nextCache->recvRequest(t);
            }
            else
            {
                outcomingRequests.push_back(t);
            }
        }
        it = evictAddresses.erase(it);
    }
}

void L2Cache::sendResponses()
{
    // std::cout << "oucomingResp.size()=" << outcomingResponses.size() << "\n";
    for (auto resp : outcomingResponses)
    {
        // store reqs dont need to response
        if (resp->fromICache)
            prevICache->recvResponse(resp);
        if (!resp->fromICache)
            prevDCache->recvResponse(resp);
    }
    outcomingResponses.clear();
}

void LLCache::process()
{
    sendResponses();
    sendRequests();

    handleRequests();
    // process pq
    Cache::process();
    handleResponses();

    cycle++;

    free_load_ports = load_ports;
    free_store_ports = store_ports;
}

void LLCache::sendRequests()
{
    // requests because of miss
    // std::cout << "outcomtingReqs.size=" << outcomingRequests.size() << "\n";
    for (auto it = outcomingRequests.begin(); it != outcomingRequests.end();)
    {
        MemTransaction *t = *it;

        uint64_t dramaddr = (t->addr / cacheLineSize) * cacheLineSize;
        if ((t->hwID != -1) && memInterface->willAcceptTransaction(dramaddr, t->isLoad))
        {
            // std::cout << "send req to mem, addr=" << dramaddr << " " << t->isLoad << " " << cycle<<"\n";
            memInterface->addTransaction(t, dramaddr, t->isLoad, cycle);
            it = outcomingRequests.erase(it);
        }
        else if ((t->hwID == -1) && memInterface->willAcceptTransaction(dramaddr, false))
        {
            // std::cout << "send evict to mem, addr=" << dramaddr << " " << cycle << "\n";
            // forwarded eviction, will be treated as just a write, nothing to do
            memInterface->addTransaction(NULL, dramaddr, false, cycle);
            it = outcomingRequests.erase(it);
        }
        else
        {
            // std::cout << "sendReq does nothing\n";
            it++;
        }
    }

    // evict
    for (auto it = evictAddresses.begin(); it != evictAddresses.end();)
    {
        uint64_t eAddr = it->first;
        bool dirty = it->second;
        auto dramaddr = (eAddr / cacheLineSize) * cacheLineSize;
        if (memInterface->willAcceptTransaction(dramaddr, false))
        {
            // std::cout << "l3 invalid=" << "cycle=" << cycle << " eAddr= " << eAddr << " tag=" << fc->getTag(eAddr) << " setid=" << fc->getSetid(eAddr) << " dity=" << dirty << "\n";
            dirty |= prevCache->invalidate(eAddr);
            dirty |= checkEvictConflict(eAddr);

            if (dirty)
                memInterface->addTransaction(NULL, dramaddr, false, cycle);
            it = evictAddresses.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void LLCache::sendResponses()
{
    for (auto resp : outcomingResponses)
    {
        // store reqs dont need to response
        prevCache->recvResponse(resp);
    }
    outcomingResponses.clear();
}

void Cache::process()
{
    for (auto it = stalledTransactions.begin(); it != stalledTransactions.end();)
    {
        MemTransaction *t = *it;
        // if MSHR is not full, erase it
        if (!enterMSHR(t))
            it = stalledTransactions.erase(it);
        else
            it++;
    }

    while (pq.size() > 0)
    {
        if (pq.top().second > cycle)
            break;
        MemTransaction *t = static_cast<MemTransaction *>(pq.top().first);
        pq.pop();

        if (t->isLoad)
        {
            read_accesses += 1;
        }
        else
        {
            write_accesses += 1;
        }

        // if (t->op && t->op->isSquashed)
        //     continue;

        bool hit = fc->access(t->addr, t->dynID, t->isLoad);
        if (hit)
        {
            // go back to previous level cache/lsu
            if (t->hwID != -1)
            {
                outcomingResponses.push_back(t);
                nhit++;
            }
        }
        else
        {
            // if (t->hwID == -1)
            //     std::cout << " cycle=" << cycle << "t->addr=" << t->addr << " tag=" << fc->getTag(t->addr) << " setid=" << fc->getSetid(t->addr) << "\n";
            // assert(t->hwID != -1);

            // t->hwID == -1 is the case that checkEvictConflist return true
            // and we ignore them here
            if (t->hwID != -1)
            {
                nmiss++;
                if (useMSHR)
                {
                    if (level == 1 && t->isPrefetch)
                    {
                        outcomingRequests.push_back(t);
                    }
                    else
                    {
                        if (enterMSHR(t))
                        {
                            stalledTransactions.push_back(t);
                        }
                    }
                }
                else
                {
                    outcomingRequests.push_back(t);
                }

                if (t->isLoad)
                {
                    read_misses += 1;
                }
                else
                {
                    write_misses += 1;
                }
            }
        }
    }
}

bool Cache::enterMSHR(MemTransaction *t)
{
    uint64_t cacheline = t->addr / cacheLineSize;
    if (mshr.find(cacheline) == mshr.end())
    {
        if (mshr.size() >= mshrSize)
        {
            // return true if full
            return true;
        }
        mshr[cacheline] = MSHREntry();
        // std::cout << "enter mshr ***" << " addr=" << t->addr << " cycle=" << cycle << " tag=" << fc->getTag(t->addr) << " setid=" << fc->getSetid(t->addr) << "\n";
        outcomingRequests.push_back(t);
    }
    mshr[cacheline].opset.push_back(t);
    return false;
}

void L1Cache::handleRequests()
{
    for (auto it = incomingRequests.begin(); it != incomingRequests.end();)
    {
        MemTransaction *req = *it;
        // if (req->op && req->op->isSquashed)
        //     continue;

        // evict req does not have op
        // std::cout << " level=" << level << "    handleRequests ***" << " dynID=" << req->op->getDynID() << " addr=" << req->addr << " cycle=" << cycle << " isload=" << req->isLoad << "\n";
        addTransaction(req, -1);
        it = incomingRequests.erase(it);

#if 1
        // prefetch
        if (req->hwID >= 0 && !isICache)
        {
            uint64_t pc = req->op->getUID();
            uint64_t addr = req->addr;

            // if pc in prefetch table
            if (prefetch_map.find(pc) == prefetch_map.end())
            {
                // if pc not in prefetch table
                prefetch_map[pc] = make_pair(req->addr, -1);
            }
            else
            {
                auto iter = prefetch_map.find(pc);
                uint64_t last_addr = iter->second.first;
                int old_stride = iter->second.second;

                int current_stride = addr - last_addr;

                if (old_stride != -1 && current_stride == old_stride)
                {
                    for (int i = 0; i < num_prefetched_lines; i++)
                    {
                        uint64_t prefetch_addr = addr + (current_stride + i) * cacheLineSize;
                        MemTransaction *prefetch_t = new MemTransaction(req->op->getDynID(), -2,
                                                                        req->op, prefetch_addr,
                                                                        true); // prefetch some distance ahead
                        prefetch_t->isPrefetch = true;
                        if (free_prefetch_ports > 0)
                        {
                            addPrefetch(prefetch_t, -1);
                        }
                        else
                        {
                            to_prefetch.push_back(prefetch_t);
                        }
                    }
                }

                prefetch_map[pc] = make_pair(addr, current_stride);
            }

            if (prefetch_map.size() > prefetch_set_size)
            {
                prefetch_map.erase(prefetch_map.begin());
            }
        }
#endif

#if 0
        if (req->hwID >= 0 && num_prefetched_lines > 0 && !isICache)
        {
            bool pattern_detected = false;
            bool single_stride = true;
            bool double_stride = true;
            int stride = min_stride;

            // detect single stride
            for (int i = 1; i < (pattern_threshold + 1); i++)
            {
                if (prefetch_set.find(req->addr - stride * i) == prefetch_set.end())
                {
                    single_stride = false;
                    break;
                }
            }
            // detect double stride
            if (!single_stride)
            {
                stride = 2 * min_stride;
                for (int i = 1; i < (pattern_threshold + 1); i++)
                {
                    if (prefetch_set.find(req->addr - stride * i) == prefetch_set.end())
                    {
                        double_stride = false;
                        break;
                    }
                }
            }
            pattern_detected = single_stride || double_stride;
            // don't prefetch or insert prefectch instrs into prefetch set
            if (!req->issuedPrefetch && !req->isPrefetch)
            {
                if (pattern_detected)
                {
                    for (int i = 0; i < num_prefetched_lines; i++)
                    {
                        MemTransaction *prefetch_t = new MemTransaction(req->op->getDynID(), -2,
                                                                        req->op, req->addr + cacheLineSize * (prefetch_distance + i),
                                                                        true); // prefetch some distance ahead
                        prefetch_t->isPrefetch = true;
                        if (free_prefetch_ports > 0)
                        {
                            addPrefetch(prefetch_t, -1);
                        }
                        else
                        {
                            to_prefetch.push_back(prefetch_t);
                        }
                    }
                    prefetch_set.insert(req->addr);
                    prefetch_set.erase(req->addr - stride * pattern_threshold);
                }
                else
                { // keep prefetch set size capped
                    int current_size = prefetch_queue.size();
                    if (current_size >= prefetch_set_size)
                    {
                        prefetch_set.erase(prefetch_queue.front());
                        prefetch_queue.pop();
                        prefetch_set_size--;
                    }
                    prefetch_set.insert(req->addr);
                    prefetch_queue.push(req->addr);
                    prefetch_set_size++;
                }
            }
        }
#endif
    }
}

void Cache::addPrefetch(MemTransaction *t, uint64_t extra_lat)
{
    pq.push(make_pair(t, cycle + latency + extra_lat));
    free_prefetch_ports--;
}

void Cache::handleRequests()
{
    for (auto it = incomingRequests.begin(); it != incomingRequests.end();)
    {
        MemTransaction *req = *it;
        // if (req->op && req->op->isSquashed)
        //     continue;

        // evict req does not have op
        // std::cout << " level=" << level << "    handleRequests ***" << " dynID=" << req->op->getDynID() << " addr=" << req->addr << " cycle=" << cycle << " isload=" << req->isLoad << "\n";
        addTransaction(req, -1);
        it = incomingRequests.erase(it);
    }
}

void Cache::handleResponses()
{
    for (auto resp : incomingResponses)
    {
        // if (resp->op && resp->op->isSquashed)
        //     continue;

        completeTransaction(resp);
        // go back to prev cache/lsu
        if (useMSHR)
        {
            // prefetch
            if (level == 1 && resp->isPrefetch)
            {
                // nothing to do
            }
            else
            {
                assert(mshr.find(resp->addr / cacheLineSize) != mshr.end());
                updateMSHR(resp);
            }
        }
        else
        {
            outcomingResponses.push_back(resp);
        }
    }
    incomingResponses.clear();
}

void Cache::addTransaction(MemTransaction *t, uint64_t extra_lat)
{
    pq.push(std::make_pair(t, cycle + latency + extra_lat));
}

// TODO: this should be removed
void Cache::flush()
{
    incomingRequests.clear();

    // clear priority queue
    std::priority_queue<TransactionOp, std::vector<TransactionOp>, TransactionOpCompare> empty;
    pq.swap(empty);
}

void Cache::completeTransaction(MemTransaction *t)
{
    int nodeId = t->dynID;
    int dirtyEvict = -1;
    int64_t evictedAddr = -1;
    uint64_t evictedOffset = 0;
    uint64_t evictedSetid = 0;

    // useless ??
    int evictedNodeId = -1;

    fc->insert(t->addr, nodeId, t->isLoad, &dirtyEvict, &evictedAddr, &evictedOffset, &evictedNodeId, &evictedSetid);

    /* we have eveicted an entry in fc */
    if (evictedAddr != -1)
    {
        assert(evictedAddr >= 0);
        // std::cout << "diraddr=" << evictedAddr << " tag=" << fc->getTag(evictedAddr) << " setid=" << fc->getSetid(evictedAddr) << " dirty=" << dirtyEvict << "\n";
        evictAddresses.push_back(std::make_pair(evictedAddr, dirtyEvict));
    }
}

bool Cache::checkEvictConflict(uint64_t address)
{

    auto copy = pq;
    while (!copy.empty())
    {
        MemTransaction *t = static_cast<MemTransaction *>(copy.top().first);
        copy.pop();

        if (fc->getSetid(address) == fc->getSetid(t->addr) && fc->getTag(address) == fc->getTag(t->addr))
        {
            // std::cout << "hwid=" << t->hwID << " addr=" << t->addr << " tag=" << fc->getTag(t->addr) << " setid=" << fc->getSetid(t->addr) << " cycle=" << copy.top().second << "\n";
            return true;
        }
    }

    return false;
}

bool Cache::invalidate(uint64_t address)
{
    // when L2 invalidate first and then L3 invalidate the same address
    // will cause fc->present = false
    bool dirty = false;

    // set dirty to true if prev CL is dirty
    if (fc->present(address))
        dirty = fc->invalidate(address);
    if (prevCache)
        dirty |= prevCache->invalidate(address);
    if (prevICache)
        dirty |= prevICache->invalidate(address);
    if (prevDCache)
        dirty |= prevDCache->invalidate(address);

    return dirty;
}

void Cache::updateMSHR(MemTransaction *t)
{
    uint64_t cacheline = t->addr / cacheLineSize;
    assert(mshr.find(cacheline) != mshr.end());
    if (mshr.find(cacheline) != mshr.end())
    {
        for (auto resp : mshr[cacheline].opset)
        {
            outcomingResponses.push_back(resp);
        }
        mshr.erase(cacheline);
    }
}

bool Cache::willAcceptTransaction(MemTransaction *t)
{
    if (!t || t->hwID == -1) // eviction or null transaction
        return free_store_ports > 0 || store_ports == -1;
    else if (t->isLoad)
        return free_load_ports > 0 || load_ports == -1;
    else if(t->isPrefetch)
        return free_prefetch_ports > 0 || cache_prefetch_ports == -1;
    else
        return free_store_ports > 0 || store_ports == -1;
}