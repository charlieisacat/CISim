#include "mem/DRAM.hpp"
#include "mem/cache.h"
#include "mem/mem_transaction.h"

using namespace std;

void DRAMSimInterface::read_complete(unsigned id,
                                     uint64_t addr,
                                     uint64_t clock_cycle)
{

  if (outstanding_read_map.find(addr) == outstanding_read_map.end())
    return;
  queue<pair<Transaction *, uint64_t>> &q = outstanding_read_map.at(addr);

  while (q.size() > 0)
  {
    pair<Transaction *, uint64_t> entry = q.front();

    MemTransaction *t = static_cast<MemTransaction *>(entry.first);
    cache->recvResponse(t);

    q.pop();
  }
  if (q.size() == 0)
    outstanding_read_map.erase(addr);
}

void DRAMSimInterface::write_complete(unsigned id, uint64_t addr, uint64_t clock_cycle)
{
  queue<pair<Transaction *, uint64_t>> &q = outstanding_write_map.at(addr);

  pair<Transaction *, uint64_t> entry = q.front();
  q.pop();

  if (q.size() == 0)
    outstanding_write_map.erase(addr);
}

void DRAMSimInterface::addTransaction(Transaction *t, uint64_t addr, bool isRead, uint64_t issueCycle)
{
  if (t != NULL)
  { // this is a LD or a ST (whereas a NULL transaction means it is really an eviction)
    // Note that caches are write-back ones, so STs also "read" from dram
    free_read_ports--;

    if (outstanding_read_map.find(addr) == outstanding_read_map.end())
    {
      outstanding_read_map.insert(make_pair(addr, queue<pair<Transaction *, uint64_t>>()));
    }

    if (use_SimpleDRAM)
    {
      // std::cout << "DRAM add transaction== addr=" << addr << "\n";
      simpleDRAM->addTransaction(false, addr);
    }
    else
    {
      mem->addTransaction(false, addr);
    }

    outstanding_read_map.at(addr).push(make_pair(t, issueCycle));
  }
  else
  { // eviction -> write into DRAM
    free_write_ports--;

    if (outstanding_write_map.find(addr) == outstanding_write_map.end())
    {
      outstanding_write_map.insert(make_pair(addr, queue<pair<Transaction *, uint64_t>>()));
    }

    if (use_SimpleDRAM)
    {
      // std::cout << "DRAM evict add transaction== addr=" << addr << "\n";
      simpleDRAM->addTransaction(true, addr);
    }
    else
    {
      mem->addTransaction(true, addr);
    }
    outstanding_write_map.at(addr).push(make_pair(t, issueCycle));
  }
}

bool DRAMSimInterface::willAcceptTransaction(uint64_t addr, bool isRead)
{
  if ((free_read_ports <= 0 && isRead && read_ports != -1) || (free_write_ports <= 0 && !isRead && write_ports != -1))
    return false;
  else if (use_SimpleDRAM)
    return simpleDRAM->willAcceptTransaction(addr);
  else
    return mem->willAcceptTransaction(addr);
}

bool DRAMSimInterface::process()
{
  free_read_ports = read_ports;
  free_write_ports = write_ports;

  if (use_SimpleDRAM)
    simpleDRAM->process();
  else
    mem->update();

  cycles++;
  return true;
}

void DRAMSimInterface::initialize(int clockspeed, int cacheline_size)
{
  // mem->setCPUClockSpeed(clockspeed * 1000000);
  simpleDRAM->initialize(clockspeed, cacheline_size);
  this->cacheline_size = cacheline_size;
}

/**
   \brief Forwards the cycle counter
 */
void DRAMSimInterface::fastForward(uint64_t inc)
{
  if (use_SimpleDRAM)
    simpleDRAM->cycles += inc;
  else
    for (uint64_t i = 0; i < inc; i++)
      mem->update();
}