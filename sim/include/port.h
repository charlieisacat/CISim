#ifndef INCLUDE_PORT_H
#define INCLUDE_PORT_H

#include "task/dyn_insn.h"
#include "function_unit.h"
#include <vector>
#include <map>
#include "params.h"

class DynamicOperation;

class Port
{
public:
    Port(uint64_t _portID, std::vector<int> _fus) : portID(_portID), fus(_fus)
    {
        fuUsage.assign(fus.size(), false);
    }

    uint64_t getPortID() { return portID; }
    
    // fu ids
    std::vector<int> fus;

    // fu is busy
    std::vector<bool> fuUsage;

    // port is blocked until this cycle  
    uint64_t releaseCycle = 0;

    void clear() { fuUsage.assign(fus.size(), false); }

private:
    // bit to indidate if function unit being used
    uint64_t portID;

};

#endif