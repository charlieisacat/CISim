#ifndef INCLUDE_ISSUE_UNIT_H
#define INCLUDE_ISSUE_UNIT_H

#include <vector>
#include "task/dyn_insn.h"
#include "window.h"
#include "port.h"
#include "lsu.h"
#include "accel.h"
#include "rou.h"

// base class
class IssueUnit
{
public:
    IssueUnit(std::shared_ptr<Params> params);

    virtual std::vector<std::shared_ptr<DynamicOperation>> issue(std::vector<std::shared_ptr<DynamicOperation>> &issueQueue,
                                                                 std::shared_ptr<Window> window,
                                                                 std::vector<std::shared_ptr<Port>> &ports,
                                                                 std::shared_ptr<LSUnit> lsu, uint64_t cycle, std::shared_ptr<RequestOrderingUnit> rou);

    bool schedule(std::shared_ptr<DynamicOperation> op, std::vector<int> &occupied, std::vector<std::shared_ptr<Port>> &ports, uint64_t cycle);

    // each cycle a port can execute a dynop, only check the port of fu is available, without blocking port and considering uops
    bool scheduleNaiveModel(std::shared_ptr<DynamicOperation> op, std::vector<int> &occupied, std::vector<std::shared_ptr<Port>> &ports, uint64_t cycle);
    // each cycle a port can execute a dynop, dynop can execute only all uops can execute, blocking port
    bool schedulePortModel(std::shared_ptr<DynamicOperation> op, std::vector<int> &occupied, std::vector<std::shared_ptr<Port>> &ports, uint64_t cycle);

    // we treat each insn as a single uop, so all ports should be available
    std::vector<int> scheduleUops(std::shared_ptr<DynamicOperation> op, std::vector<int> &occupied, std::vector<std::shared_ptr<Port>> ports, uint64_t cycle);

    std::shared_ptr<Port> getAvailablePort(int fu,
                                           std::vector<int> allowPorts,
                                           std::vector<std::shared_ptr<Port>> &ports,
                                           int &avaliableFuID,
                                           std::vector<int> occupied,
                                           uint64_t cycle);

    uint64_t fuStall = 0;

    std::map<int, int> fuAvalMap;
    // used for flush
    std::map<int, int> fuAvalMap_recover;

    std::vector<int> sortFUs(std::vector<int> fus);

    void freeFU(std::vector<int> usingFuIDs,
                std::vector<std::shared_ptr<Port>> usingPorts);

    void occupyFU(std::vector<int> usingFuIDs,
                std::vector<std::shared_ptr<Port>> usingPorts);

    bool naive = false;

    // fu is fully pipelined
    std::map<int, int> fuPipeMap;

    int getOccupiedCstmFU();
};

#endif