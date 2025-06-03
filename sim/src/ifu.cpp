#include "ifu.h"

void IFU::IF0(std::vector<std::shared_ptr<DynamicOperation>> &FTQ)
{
    if (FTQ.size() == 0)
        return;

    pfPtr = (pfPtr + 1) % 32;

    // will never occupied for the first request
    if (!occupied)
    {
        auto op = FTQ[0];
        MemTransaction *req = new MemTransaction(op->getDynID(), 0, op, op->getUID(), true, true);

        if (cache->willAcceptTransaction(req))
        {
            sendReqToCache(req);

            // remove the op from FTQ

            // cout << "FTQ erase = " << FTQ[0]->getUID() << "\n";
            FTQ.erase(FTQ.begin());

            // since the front item is removed
            pfPtr--;
        }
    }

    // TODO: move to ICache
    // prefetch (fetch directed prefetching)
    if (FTQ.size() >= 1 && pfPtr < FTQ.size())
    {
        auto nextOp = FTQ[pfPtr];

        // cancel prefetch if addr has been requested
        if(count(prefetchedAddrs.begin(), prefetchedAddrs.end(), nextOp->getUID()))
        {
            return;
        }

        if (prefetchedAddrs.size() >= 32)
        {
            prefetchedAddrs.erase(prefetchedAddrs.begin());
        }
        prefetchedAddrs.push_back(nextOp->getUID());
        MemTransaction *prefetch_t = new MemTransaction(nextOp->getDynID(), -2,
                                                        nextOp, nextOp->getUID(), true, true);
        prefetch_t->isPrefetch = true;
        if (cache->willAcceptTransaction(prefetch_t))
        {
            sendReqToCache(prefetch_t);
        }
    }
}

void IFU::sendReqToCache(MemTransaction *req)
{
    // IFU can only handle one request at a time
    occupied = true;
    // recvResponse(req);
    cache->recvRequest(req);
}

void IFU::IF1(std::vector<std::shared_ptr<DynamicOperation>> &processedOps,
              std::vector<std::shared_ptr<DynamicOperation>> &insnQueue)
{
    assert(responses.size() <= 1);

    if (responses.size())
    {
        auto resp = responses[0];
        if (!resp->op || resp->op->isSquashed)
        {
            return;
        }

        uint64_t uid = resp->op->getUID();

        assert(processedOps.size() >= 0);
        // cout << "resp dynid=" << resp->op->getDynID()<< " name="<<resp->op->getOpName()<<" uid="<<resp->op->getUID() << "\n";
        // cout << "front dynid=" << processedOps.front()->getDynID()<< " name="<<processedOps.front()->getOpName()<<" uid="<<processedOps.front()->getUID() << "\n";
        // cout<<processedOps.front()->getUID()<<" uid="<<uid<<"\n";
        assert(processedOps.front()->getUID() == uid);

        // add ops to insnQueue from processedOps
        for (int i = 0; i < packageSize; i++)
        {
            if (processedOps.size() == 0)
                break;

            // cout << " add uid=" << processedOps[0]->getUID() << " uid+i=" << uid + i << " uid=" << uid << " dynid=" << processedOps[0]->getDynID() << "\n";
            // instructions should be consecutive
            if (processedOps[0]->getUID() != uid + i)
            {
                // cout<<"---------- break\n";
                break;
            }
            // cout<<"---------- erase\n";

            insnQueue.push_back(processedOps[0]);

            // remove the op from processedOps
            processedOps.erase(processedOps.begin());
        }

        responses.clear();
        occupied = false;
    }
}