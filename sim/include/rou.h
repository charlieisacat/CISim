#ifndef INCLUDE_ROU_H
#define INCLUDE_ROU_H

#include "task/dyn_insn.h"
#include <unordered_map>
#include <vector>

using namespace std;

class RequestOrderingUnit
{
public:
    RequestOrderingUnit() {}
    unordered_map<uint64_t, vector<uint64_t>> reorderBuffer;
    void addInstruction(shared_ptr<DynamicOperation> insn);
    bool canIssue(shared_ptr<DynamicOperation> insn);
    void releaseEntry(shared_ptr<DynamicOperation> insn);
};

#endif
