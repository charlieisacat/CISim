#include "lsu.h"
#include <iostream>

void LSUnit::flush()
{
    LDQ->flush();
    STQ->flush();
}

void LSUnit::issue(std::shared_ptr<DynamicOperation> op)
{
    assert(op->status == DynOpStatus::ALLOCATED);
    op->updateStatus();
}

void LSUnit::addInstruction(std::shared_ptr<DynamicOperation> op)
{
    if (op->isLoad())
        LDQ->addInstruction(op);
    if (op->isStore())
        STQ->addInstruction(op);
}

void LSUnit::commitInstruction(std::shared_ptr<DynamicOperation> op)
{
    if (op->isLoad())
        LDQ->commitInstruction(op);

    if (op->isStore())
        STQ->commitInstruction(op);
}

bool LSUnit::shouldFlush(std::shared_ptr<DynamicOperation> op)
{
    uint64_t dynID = op->getDynID();
    uint64_t addr = op->getAddr();
    // std::cout << "ldqsize=" << LDQ->queue.size() << "\n";
    // std::cout << "stqsize=" << STQ->queue.size() << "\n";

    for (auto entry : LDQ->queue)
    {
        if (entry->empty)
            continue;

        if (entry->dynID > dynID && entry->address == addr)
        {
            auto lop = entry->op;
            if (entry->dataForwarded)
            {
                assert(entry->forwardOp);
                // load is forwared from an older address
                if (entry->forwardOp->getDynID() < op->getDynID())
                {
                    return true;
                }
            }
            else
            {
                // if dataforwared == true, status will be sleep
                if (lop->status >= DynOpStatus::EXECUTING)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

void LSUnit::sendReqToCache(MemTransaction *req)
{
#if 1
    cache->recvRequest(req);
#else // disable cache
    if (req->isLoad)
        recvResponse(req);
#endif
}

void LSUnit::wb()
{
    for (auto resp : responses)
    {
        if (resp->op && !resp->op->isSquashed)
        {
            LDQ->completeInstruction(resp->op);
        }
    }
    responses.clear();
}

int LSUnit::getExecutingLoads()
{
    int n = 0;
    for (auto entry : LDQ->queue)
    {
        if (entry->empty)
            continue;

        if (entry->op->status == DynOpStatus::EXECUTING)
        {
            n++;
        }
    }
    return n;
}

void LSUnit::doForwardData(std::shared_ptr<LDSTQEntry> entry)
{
    for (auto lop : entry->forwardList)
    {
        assert(lop->status == DynOpStatus::SLEEP);
        LDQ->completeInstruction(lop);
    }
    entry->forwardList.clear();
}

void LSUnit::process()
{
    for (auto entry : STQ->queue)
    {
        if (entry->empty)
            continue;

        auto op = entry->op;
        // hwID is 0, because we just have on cpu core now
        if (op->status == DynOpStatus::COMMITED)
        {
            MemTransaction *req = new MemTransaction(op->getDynID(), 0, op, op->getAddr(), op->isLoad());
            if (cache->willAcceptTransaction(req))
            {
                sendReqToCache(req);
                STQ->freeEntry(op);
            }

        }

        if (op->status >= DynOpStatus::READY)
        {
            doForwardData(entry);
        }
    }

    for (auto entry : LDQ->queue)
    {
        if (entry->empty)
            continue;

        if (entry->op->status == DynOpStatus::ISSUED)
        {
            auto op = entry->op;

            // perfect data fwd
            std::shared_ptr<LDSTQEntry> fwdEntry = forwardData(op);
            if (fwdEntry)
            {
                entry->dataForwarded = true;
                op->updateStatus(DynOpStatus::SLEEP);
                entry->forwardOp = fwdEntry->op;

                if (fwdEntry->op->status >= DynOpStatus::READY)
                {
                    op->setFinish(true);
                }
                else
                {
                    fwdEntry->forwardList.push_back(op);
                }
            }
            else
            {
                MemTransaction *req = new MemTransaction(op->getDynID(), 0, op, op->getAddr(), op->isLoad());
                if (cache->willAcceptTransaction(req))
                {
                    sendReqToCache(req);
                    op->updateStatus();
                }
            }
        }
        else if (entry->op->status == DynOpStatus::COMMITED)
        {
            LDQ->freeEntry(entry->op);
        }
    }
}

bool LSUnit::canDispatch(std::shared_ptr<DynamicOperation> op, bool cpu)
{
    if(cpu)
    {
    if (op->isLoad())
        return !ifLDQFull();
    if (op->isStore())
        return !ifSTQFull();
    return true;
    }

    return !ifLDQFull() && !ifSTQFull();
}

std::shared_ptr<LDSTQEntry> LSUnit::forwardData(std::shared_ptr<DynamicOperation> op)
{
    std::shared_ptr<LDSTQEntry> ret = nullptr;
    uint64_t dynID = 0;

    for (auto entry : STQ->queue)
    {
        if (entry->empty)
            continue;

        auto sop = entry->op;
        if (sop->getAddr() == op->getAddr() &&
            sop->getDynID() < op->getDynID())
        {
            assert(sop->getDynID() != dynID);
            // get the youngest store insn
            if (sop->getDynID() > dynID)
            {
                ret = entry;
                dynID = sop->getDynID();
            }
        }
    }

    return ret;
}

std::shared_ptr<LDSTQEntry> LSUnit::getAliasOp(std::shared_ptr<DynamicOperation> op)
{
    std::shared_ptr<LDSTQEntry> aliasEntry = nullptr;
    uint64_t dynID = 0;

    for (auto entry : STQ->queue)
    {
        if (entry->empty)
            continue;

        // store op
        auto sop = entry->op;

        // ready op will be directly forwared
        if (sop->status >= DynOpStatus::READY)
            continue;

        if (sop->getAddr() == op->getAddr() &&
            sop->getDynID() < op->getDynID())
        {
            assert(sop->getDynID() != dynID);
            if (sop->getDynID() > dynID)
            {
                aliasEntry = entry;
                dynID = sop->getDynID();
            }
        }
    }

    return aliasEntry;
}
