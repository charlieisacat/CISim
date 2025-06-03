#ifndef MEM_TRANSACTION_H
#define MEM_TRANSACTION_H
#include <vector>
#include <memory>
#include <assert.h>
#include <utility>

#include "task/dyn_insn.h"
class Transaction;

// transction, cycle
typedef std::pair<Transaction *, uint64_t> TransactionOp;
class TransactionOpCompare
{
public:
    /** \brief   */
    bool operator()(const TransactionOp &l, const TransactionOp &r) const
    {
        if (r.second < l.second)
            return true;
        else
            return false;
    }
};

class Transaction
{
public:
    Transaction() {}
};

class MemTransaction : public Transaction
{
public:
    MemTransaction(int _dynID, int _hwID, std::shared_ptr<DynamicOperation> _op,
                   uint64_t _addr, bool _isLoad, bool _fromICache = false) : dynID(_dynID), hwID(_hwID), op(_op),
                                                                             addr(_addr), isLoad(_isLoad), fromICache(_fromICache) {}
    std::shared_ptr<DynamicOperation> op = nullptr;
    uint64_t addr;
    bool isLoad;
    int dynID = -1;
    bool fromICache = false;

    /** \brief   */
    bool isPrefetch = false;
    /** \brief   */
    bool issuedPrefetch = false;

    // -1 is evict
    // >=0 represents different hardware instance 
    int hwID = -2;
};

#endif