#pragma once

#include <unordered_map>
#include <queue>
#include "assert.h"

#include "DRAMSim.hpp"
#include "SimpleDRAM.hpp"
#include "cache.h"

class Cache;
class SimpleDRAM;
using namespace std;

class DRAMSimInterface
{
public:
  // LLCache
  std::shared_ptr<Cache> cache;
  bool ideal; // not used
  uint64_t cycles = 0;
  int read_ports;
  int write_ports;
  int free_read_ports;
  int free_write_ports;
  SimpleDRAM *simpleDRAM;
  bool use_SimpleDRAM;
  unordered_map<uint64_t, queue<pair<Transaction *, uint64_t>>> outstanding_read_map;
  unordered_map<uint64_t, queue<pair<Transaction *, uint64_t>>> outstanding_write_map;
  DRAMSim::MultiChannelMemorySystem *mem;
  int cacheline_size;
  // bool finished_acc = false;
  int LLC_evicted = false;
  // uint64_t acc_final_cycle = 0;
  DRAMSimInterface(std::shared_ptr<Cache> cache, bool use_SimpleDRAM,
                   bool ideal, int read_ports, int write_ports,
                   string DRAM_system, string DRAM_device) : cache(cache), use_SimpleDRAM(use_SimpleDRAM),
                                                             ideal(ideal), read_ports(read_ports), write_ports(write_ports)
  {

    // create DRAMsim2 object
#if 0
    DRAMSim::TransactionCompleteCB *read_cb = new DRAMSim::Callback<DRAMSimInterface, void, unsigned, uint64_t, uint64_t>(this, &DRAMSimInterface::read_complete);
    DRAMSim::TransactionCompleteCB *write_cb = new DRAMSim::Callback<DRAMSimInterface, void, unsigned, uint64_t, uint64_t>(this, &DRAMSimInterface::write_complete);
    mem = DRAMSim::getMemorySystemInstance(DRAM_device, DRAM_system, "", "simulator", 65536);
    mem->RegisterCallbacks(read_cb, write_cb, NULL);
#endif

    free_read_ports = read_ports;
    free_write_ports = write_ports;
  }
  void read_complete(unsigned id, uint64_t addr, uint64_t clock_cycle);
  void write_complete(unsigned id, uint64_t addr, uint64_t clock_cycle);
  void addTransaction(Transaction *t, uint64_t addr, bool isRead, uint64_t issueCycle);
  bool willAcceptTransaction(uint64_t addr, bool isRead);
  bool process();

  void initialize(int clockspeed, int cacheline_size);
  void fastForward(uint64_t inc);
};
