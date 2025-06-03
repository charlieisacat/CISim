#include "rou.h"

#include <cstdint>

std::size_t hashTwoInts(int a, int b) {
    // Combine the two integers into a single hash value
    std::size_t hash = 0;
    hash = a;
    hash ^= (b + 0x9e3779b9 + (hash << 6) + (hash >> 2));
    return hash;
}

void RequestOrderingUnit::addInstruction(shared_ptr<DynamicOperation> insn)
{
    if (insn->isExtractFunc || insn->isPackingFunc)
    {
        uint64_t dynID = insn->getDynID();

        auto hash = hashTwoInts(insn->dyserUnitID, insn->offset);

        if (!reorderBuffer.count(hash))
        {
            reorderBuffer[hash] = vector<uint64_t>();
        }
        reorderBuffer[hash].push_back(dynID);

        // cout << "isExt=" << insn->isExtractFunc << " ispa=" << insn->isPackingFunc << " uid="
        //      << uid << " opname=" << insn->getOpName() << " dynid=" << insn->getDynID() << "\n";
    }
}

bool RequestOrderingUnit::canIssue(shared_ptr<DynamicOperation> insn)
{
    if (insn->isExtractFunc || insn->isPackingFunc)
    {
        uint64_t dynID = insn->getDynID();

        auto hash = hashTwoInts(insn->dyserUnitID, insn->offset);
        // cout << "dyser check========== " << uid << " dynid=" << dynID << "\n";

        assert(reorderBuffer.count(hash));
        assert(reorderBuffer[hash].size());

        auto begin = reorderBuffer[hash].begin();
        if (*begin == dynID)
        {
            return true;
        }

        return false;
    }

    return true;
}

void RequestOrderingUnit::releaseEntry(shared_ptr<DynamicOperation> insn)
{
    if (insn->isExtractFunc || insn->isPackingFunc)
    {
        uint64_t dynID = insn->getDynID();

        auto hash = hashTwoInts(insn->dyserUnitID, insn->offset);

        assert(reorderBuffer.count(hash));
        assert(reorderBuffer[hash].size());
        // cout << "dyser release========== " << uid << " dynid=" << dynID << "\n";

        auto begin = reorderBuffer[hash].begin();
        if (*begin == dynID)
        {
            reorderBuffer[hash].erase(begin);
        }
    }
}
