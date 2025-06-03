#include "issue_unit.h"

IssueUnit::IssueUnit(std::shared_ptr<Params> params)
{
    // std::cout << "Not pipelined fu size=" << params->notPipelineFUs.size() << "\n";
    for (auto config : params->portConfigs)
    {
        for (auto fu : config.second)
        {
            if (fuAvalMap.find(fu) == fuAvalMap.end())
            {
                fuAvalMap[fu] = 0;
            }
            fuAvalMap[fu]++;

            if (std::find(params->notPipelineFUs.begin(), params->notPipelineFUs.end(), fu) != params->notPipelineFUs.end())
            {
                fuPipeMap[fu] = 0;
            }
            else
            {
                fuPipeMap[fu] = 1;
            }
        }
    }

    fuAvalMap_recover = fuAvalMap;
}

void IssueUnit::occupyFU(std::vector<int> usingFuIDs,
                         std::vector<std::shared_ptr<Port>> usingPorts)
{
    for (int i = 0; i < usingFuIDs.size(); i++)
    {
        int fuID = usingFuIDs[i];
        auto port = usingPorts[i];
        if (!fuPipeMap[port->fus[fuID]])
        {
            port->fuUsage[fuID] = true;
        }
    }
}

void IssueUnit::freeFU(std::vector<int> usingFuIDs,
                       std::vector<std::shared_ptr<Port>> usingPorts)
{
    for (int i = 0; i < usingFuIDs.size(); i++)
    {
        auto port = usingPorts[i];
        int fuID = usingFuIDs[i];
        if (!fuPipeMap[port->fus[fuID]])
        {
            port->fuUsage[usingFuIDs[i]] = false;
            fuAvalMap[port->fus[fuID]] += 1;
        }
    }
}

std::shared_ptr<Port> IssueUnit::getAvailablePort(int fu,
                                                  std::vector<int> allowPorts,
                                                  std::vector<std::shared_ptr<Port>> &ports,
                                                  int &avaliableFuID,
                                                  std::vector<int> occupied,
                                                  uint64_t cycle)
{

    for (auto port : ports)
    {
        if (std::find(allowPorts.begin(), allowPorts.end(), port->getPortID()) == allowPorts.end())
            continue;

        if (std::find(occupied.begin(), occupied.end(), port->getPortID()) != occupied.end())
            continue;

        if (!naive && port->releaseCycle > cycle)
            continue;

        auto fuSize = port->fus.size();
        for (int i = 0; i < fuSize; i++)
        {
            if (fu == port->fus[i])
            {
                if (port->fuUsage[i] == false)
                {
                    // set the position in vector of fus
                    avaliableFuID = i;
                    // return which port the fu belongs to
                    return port;
                }
            }
        }
    }

    return nullptr;
}

std::vector<int> IssueUnit::sortFUs(std::vector<int> fus)
{
    std::vector<int> sortedFUs;
    std::vector<std::pair<int, int>> fuWithPriority;

    // less count, higer priority
    for (size_t i = 0; i < fus.size(); ++i)
    {
        fuWithPriority.push_back(std::make_pair(fuAvalMap[fus[i]], fus[i]));
    }
    std::sort(fuWithPriority.begin(), fuWithPriority.end(), [](const std::pair<int, int> &a, const std::pair<int, int> &b)
              { return a.first < b.first; });

    for (size_t i = 0; i < fuWithPriority.size(); ++i)
    {
        sortedFUs.push_back(fuWithPriority[i].second);
    }

    return sortedFUs;
}

std::vector<int> IssueUnit::scheduleUops(std::shared_ptr<DynamicOperation> op, std::vector<int> &occupied, std::vector<std::shared_ptr<Port>> ports, uint64_t cycle)
{
    assert(!naive);

    std::vector<int> tmpOccupied;
    for (auto mapping : op->portMapping)
    {
        // for a uop
        for (auto portID : mapping.second)
        {
            assert(portID == ports[portID]->getPortID());
            // not occupied by other uops of this dyn op
            if (std::find(tmpOccupied.begin(), tmpOccupied.end(), portID) != tmpOccupied.end())
                continue;

            // not occupied by other dyn op
            if (std::find(occupied.begin(), occupied.end(), portID) != occupied.end())
                continue;

            // port is not blocked
            if (ports[portID]->releaseCycle > cycle)
                continue;

            // the port is avaliable
            tmpOccupied.push_back(portID);
            break;
        }
    }

    assert(tmpOccupied.size() <= op->portMapping.size());
    if (tmpOccupied.size() == op->portMapping.size())
    {
        occupied.insert(occupied.end(), tmpOccupied.begin(), tmpOccupied.end());
        return tmpOccupied;
    }

    return {};
}

bool IssueUnit::schedulePortModel(std::shared_ptr<DynamicOperation> op, std::vector<int> &occupied, std::vector<std::shared_ptr<Port>> &ports, uint64_t cycle)
{
    std::vector<int> portIDs = scheduleUops(op, occupied, ports, cycle);
    if (portIDs.size() == 0)
    {
        return false;
    }

    std::vector<std::shared_ptr<Port>> workPorts;
    for (auto id : portIDs)
        workPorts.push_back(ports[id]);

    std::vector<int> avalIDs;
    std::vector<std::shared_ptr<Port>> avalPorts;
    std::vector<int> tmpOccupied;

    bool ret = false;
    // sort fu according to priority
    for (auto fus : op->fus)
    {
        bool portAva = true;
        auto sortedFUs = sortFUs(fus);

        for (auto fu : sortedFUs)
        {
            int avaliableFuID = -1;

            // when blocking port, only find the fu in those available ports
            std::shared_ptr<Port> port = getAvailablePort(fu, op->getAllowPorts(), workPorts, avaliableFuID, tmpOccupied, cycle);

            // port is available but fu is stil busy
            if (port == nullptr)
            {
                ret = false;
                portAva = false;
                break;
            }

            if (!fuPipeMap[fu])
            {
                avalIDs.push_back(avaliableFuID);
                avalPorts.push_back(port);
            }

            tmpOccupied.push_back(port->getPortID());
        }

        if (portAva)
        {
            ret = true;
            occupied.insert(occupied.end(), portIDs.begin(), portIDs.end());

            // block port which will be released in releaseCycle
            for (int i = 0; i < workPorts.size(); i++)
            {
                auto port = workPorts[i];
                port->releaseCycle = cycle + op->portStallCycles[i];
            }

            // decrement available fu count
            for (int i = 0; i < avalIDs.size(); i++)
            {
                int id = avalIDs[i];
                auto port = avalPorts[i];

                assert(fuAvalMap.find(port->fus[id]) != fuAvalMap.end());
                // std::cout << "i=" << i << " portid=" << port->getPortID() << " id=" << id << " fu=" << port->fus[id] << " n=" << fuAvalMap[port->fus[id]] << "\n";
                assert(fuAvalMap[port->fus[id]] > 0);
                if (!fuPipeMap[port->fus[id]])
                    fuAvalMap[port->fus[id]]--;
            }

            op->occupyFU(avalIDs, avalPorts);
            occupyFU(avalIDs, avalPorts);

            break;
        }
    }

    return ret;
}

bool IssueUnit::scheduleNaiveModel(std::shared_ptr<DynamicOperation> op, std::vector<int> &occupied, std::vector<std::shared_ptr<Port>> &ports, uint64_t cycle)
{
    bool ret = false;
    for (auto fus : op->fus)
    {
        bool portAva = true;
        // sort fu according to priority
        auto sortedFUs = sortFUs(fus);

        std::vector<int> tmpOccupied = occupied;
        std::vector<int> avalIDs;
        std::vector<std::shared_ptr<Port>> avalPorts;
        for (auto fu : sortedFUs)
        {
            int avaliableFuID = -1;
            // find which port have fu and is avaliable
            // considering port mapping
            std::shared_ptr<Port> port = getAvailablePort(fu, op->getAllowPorts(), ports, avaliableFuID, tmpOccupied, cycle);

            if (port == nullptr)
            {
                ret = false;
                portAva = false;
                break;
            }

            if (!fuPipeMap[fu])
            {
                avalIDs.push_back(avaliableFuID);
                avalPorts.push_back(port);
            }

            tmpOccupied.push_back(port->getPortID());
        }

        if (portAva)
        {
            ret = true;
            for (int i = 0; i < avalIDs.size(); i++)
            {
                int id = avalIDs[i];
                auto port = avalPorts[i];
                occupied.push_back(port->getPortID());

                assert(fuAvalMap.find(port->fus[id]) != fuAvalMap.end());
                // std::cout << "i=" << i << " portid=" << port->getPortID() << " id=" << id << " fu=" << port->fus[id] << " n=" << fuAvalMap[port->fus[id]] << "\n";
                assert(fuAvalMap[port->fus[id]] > 0);
                if (!fuPipeMap[port->fus[id]])
                    fuAvalMap[port->fus[id]]--;
            }

            op->occupyFU(avalIDs, avalPorts);
            occupyFU(avalIDs, avalPorts);

            break;
        }
    }

    return ret;
}

bool IssueUnit::schedule(std::shared_ptr<DynamicOperation> op, std::vector<int> &occupied, std::vector<std::shared_ptr<Port>> &ports, uint64_t cycle)
{
    if (naive)
        return scheduleNaiveModel(op, occupied, ports, cycle);

    return schedulePortModel(op, occupied, ports, cycle);
}

// port will not be blocked
// dynamic priority, less avaliable fus higher priority
std::vector<std::shared_ptr<DynamicOperation>> IssueUnit::issue(std::vector<std::shared_ptr<DynamicOperation>> &issueQueue,
                                                                         std::shared_ptr<Window> window,
                                                                         std::vector<std::shared_ptr<Port>> &ports,
                                                                         std::shared_ptr<LSUnit> lsu, uint64_t cycle, std::shared_ptr<RequestOrderingUnit> rou)
{

    std::vector<std::shared_ptr<DynamicOperation>> readyInsns;
    // occupied ports
    std::vector<int> occupied;

    for (auto it = issueQueue.begin(); it != issueQueue.end();)
    {
        std::shared_ptr<DynamicOperation> insn = *it;

        // check data dependency
        if (window->canIssue(insn) && rou->canIssue(insn))
        {
            // some insns are already in READY
            if (insn->status == DynOpStatus::ALLOCATED)
            {
                insn->updateStatus();
            }

            // insn whose avaliable fus requiring same port won't be issued
            // 1. check port is avaliable
            // 2. schedule
            if (schedule(insn, occupied, ports, cycle))
            {
                assert(insn->status == DynOpStatus::READY);
                insn->updateStatus();
                insn->e = cycle;
                readyInsns.push_back(insn);

                it = issueQueue.erase(it);
                window->decrementIssueQueueUsed();

                rou->releaseEntry(insn);
            }
            else
            {
                fuStall++;
                // std::cout << "cycle=" << cycle << " dynid=" << insn->getDynID() << " opname=" << insn->getOpName() << " not schedule \n";
                it++;
            }
        }
        else
        {
            // std::cout << "cycle=" << cycle << " dynid=" << insn->getDynID() << " opname=" << insn->getOpName() << " not data dep \n";
            it++;
        }

        if (occupied.size() == ports.size())
            break;
    }

    return readyInsns;
}
