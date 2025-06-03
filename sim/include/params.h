#ifndef INCLUDE_PARAMS_H
#define INCLUDE_PARAMS_H
#include <cstdint>
#include <map>
#include <string>
#include "task/llvm.h"
#include "function_unit.h"
#include "yaml-cpp/yaml.h"
#include "branch_predictor.h"

struct InstConfig
{
    int opcode_num;
    int runtime_cycles;
    int encap_n = 0;
    std::vector<std::pair<int, std::vector<int>>> ports;
    std::vector<std::vector<int>> fus;
};

struct Params
{
public:
    uint64_t windowSize = 1;
    uint64_t retireSize = 1;
    uint64_t robSize = 128;
    uint64_t issueQSize = 128;
    std::map<uint64_t, std::shared_ptr<InstConfig>> insnConfigs;
    std::map<int, std::vector<int>> portConfigs;
    std::vector<int> notPipelineFUs;
    bool simpleDram = false;
    int flushPenalty = 1;
    int clockSpeed = 2100;
    int ldqSize = 128;
    int stqSize = 128;

    // l1cache
    int l1_latency = 1;
    int l1_CLSize = 1;
    int l1_size = 1;
    int l1_assoc = 1;
    int l1_mshr = 0;
    int l1_store_ports = 8;
    int l1_load_ports= 8;

    // l2cache
    int l2_latency = 1;
    int l2_CLSize = 1;
    int l2_size = 1;
    int l2_assoc = 1;
    int l2_mshr = 0;
    int l2_store_ports = 8;
    int l2_load_ports= 8;

    // l3cache
    int l3_latency = 1;
    int l3_CLSize = 1;
    int l3_size = 1;
    int l3_assoc = 1;
    int l3_mshr = 0;
    int l3_store_ports = 8;
    int l3_load_ports= 8;

    // dram
    int dram_latency = 100;
    int dram_bw = 10; // GB/sec

    std::string DRAM_system = "";
    std::string DRAM_device = "";

    bool naive = false;

    // bp
    int bhtSize = 4;
    TypeBpred bpType = bp_perfect;

    // number of bits for the global history register
    int ghrSize = 10;

    // dynamic fu count
    int threshold = -1;

    std::string offload_func_name = "";
    bool acc_speculative = 0;

    bool dyser = 0;

    int lsu_port = 0;
};

#endif