#include "core.h"
#include "function_unit.h"

#include <stdio.h>
#include <iostream>

#include <chrono>
#include <fstream>

std::string insn_to_string_1(llvm::Value *v)
{
    std::string str;
    llvm::raw_string_ostream rso(str);
    rso << *v;        // This is where the instruction is serialized to the string.
    return rso.str(); // Return the resulting string.
}

Core::Core(std::shared_ptr<SIM::Task> _task, std::shared_ptr<Params> _params) : Hardware(_task, _params)
{
#if 0
    for (auto item : params->insnConfigs)
    {
        auto inst = item.second;
        std::cout << inst->opcode_num << " port:";
        for (auto item : inst->ports)
        {
            std::cout << "num=" << item.first << " ";
            for (auto port : item.second)
            {
                std::cout << port << " ";
            }
            std::cout << "\n";
        }
    }
#endif

    offload_func_name = params->offload_func_name;
    speculative = params->acc_speculative;
    dyser = params->dyser;
}

void Core::initialize()
{

    window = std::make_shared<Window>(params->windowSize, params->retireSize, params->robSize, params->issueQSize);
    issueUnit = std::make_shared<IssueUnit>(params);
    l1c = std::make_shared<L1Cache>(params->l1_latency, params->l1_CLSize, params->l1_size, params->l1_assoc, params->l1_mshr, params->l1_store_ports, params->l1_load_ports);
    l2c = std::make_shared<L2Cache>(params->l2_latency, params->l2_CLSize, params->l2_size, params->l2_assoc, params->l2_mshr, params->l2_store_ports, params->l2_load_ports);
    lsu = std::make_shared<LSUnit>(l1c, params->ldqSize, params->stqSize);

    l1i = std::make_shared<L1Cache>(params->l1_latency, params->l1_CLSize, params->l1_size, params->l1_assoc, params->l1_mshr, params->l1_store_ports, params->l1_load_ports, true);
    ifu = make_shared<IFU>(l1i);

    lsu->lsu_port = params->lsu_port;

    // make sure enough insns are in insn queue
    insnQueueMaxSize = 64;

    issueUnit->naive = params->naive;

    // lsu and l1cache
    l1c->lsu = lsu;
    l1c->nextCache = l2c;

    l1i->ifu = ifu;
    l1i->nextCache = l2c;

    l2c->prevDCache = l1c;
    l2c->prevICache = l1i;

    for (auto item : params->portConfigs)
    {
        int portID = item.first;
        // std::cout << "portID=" << portID << "\n";
        ports.push_back(std::make_shared<Port>(portID, item.second));
    }

    flushPenalty = params->flushPenalty;
    bp = std::make_shared<Bpred>(params->bpType, params->bhtSize, params->ghrSize);
    stat = std::make_shared<Statistic>();

    pam = new PowerAreaModel();

    targetBB = fetchEntryBB();

    acc = make_shared<Accelerator>(task, params);
    acc->lsu = lsu;
    acc->hwHost = this;

    // dyserUnit = make_shared<DyserUnit>(task, params, dyserEntryBB);
    rou = make_shared<RequestOrderingUnit>();
}

void Core::setDyserEntry(unordered_map<int, shared_ptr<SIM::Function>> dyser_func_map)
{
    // dyserEntryBB = entry;
    for (auto item = dyser_func_map.begin(); item != dyser_func_map.end(); item++)
    {
        int opcode = item->first;
        auto dyserUnit = make_shared<DyserUnit>(task, params, item->second->getEntryBB());
        dyserUnits.insert({opcode, dyserUnit});
    }
}

void Core::finish()
{
}

void Core::printForMcpat()
{
    std::ofstream out_file("mcpat.txt");
    uint64_t total_instructions = window->getTotalCommit();

    const std::set<uint64_t> int_opcodes = {13, 15, 34, 53, 48, 47, 25, 26, 27, 28, 29, 30, 17, 19, 20, 23, 22};
    const std::set<uint64_t> fp_opcodes = {18, 21, 24, 14, 16, 54, 46, 42, 41, 45, 43};
    uint64_t int_instructions = 0;
    uint64_t fp_instructions = 0;
    uint64_t branch_instructions = 0;
    uint64_t load_instructions = 0;
    uint64_t store_instructions = 0;
    uint64_t function_calls = 0;

    for (auto item : stat->insnCountMap)
    {
        llvm::Instruction *insn = item.first;
        uint64_t count = item.second;

        uint32_t opcode = insn->getOpcode();
        if (int_opcodes.count(opcode))
        {
            int_instructions += count;
        }
        else if (fp_opcodes.count(opcode))
        {
            fp_instructions += count;
        }
        else if (opcode == 2 || opcode == 3)
        {
            branch_instructions += count;
        }
        else if (opcode == 32)
        {
            load_instructions += count;
        }
        else if (opcode == 33)
        {
            store_instructions += count;
        }
        else if (opcode == 56 || opcode == 5)
        {
            function_calls += count;
        }
    }

    uint64_t branch_misprediction = window->mispredictNum;

    out_file << "total_instructions=" << total_instructions << "\n";
    out_file << "int_instructions=" << int_instructions << "\n";
    out_file << "fp_instructions=" << fp_instructions << "\n";
    out_file << "branch_instructions=" << branch_instructions << "\n";
    out_file << "branch_misprediction=" << branch_misprediction << "\n";
    out_file << "load_instructions=" << load_instructions << "\n";
    out_file << "store_instructions=" << store_instructions << "\n";
    out_file << "committed_instructions=" << total_instructions << "\n";
    out_file << "committed_int_instructions=" << int_instructions << "\n";
    out_file << "committed_fp_instructions=" << fp_instructions << "\n";

    uint64_t total_cycles = cycles;

    out_file << "committed_fp_instructions=" << fp_instructions << "\n";

    uint64_t ROB_reads = total_instructions;
    uint64_t ROB_writes = total_instructions;
    out_file << "ROB_reads=" << ROB_reads << "\n";
    out_file << "ROB_writes=" << ROB_writes << "\n";

    uint64_t inst_window_reads = total_instructions - fp_instructions;
    uint64_t inst_window_writes = total_instructions - fp_instructions;
    uint64_t inst_window_wakeup_accesses = total_instructions - fp_instructions;
    uint64_t fp_inst_window_reads = fp_instructions;
    uint64_t fp_inst_window_writes = fp_instructions;
    uint64_t fp_inst_window_wakeup_accesses = fp_instructions;
    out_file << "inst_window_reads=" << inst_window_reads << "\n";
    out_file << "inst_window_writes=" << inst_window_writes << "\n";
    out_file << "inst_window_wakeup_accesses=" << inst_window_wakeup_accesses << "\n";
    out_file << "fp_inst_window_reads=" << fp_inst_window_reads << "\n";
    out_file << "fp_inst_window_writes=" << fp_inst_window_writes << "\n";
    out_file << "fp_inst_window_wakeup_accesses=" << fp_inst_window_wakeup_accesses << "\n";

    uint64_t int_regfile_reads = stat->intRFReads;
    uint64_t int_regfile_writes = stat->intRFWrites;
    uint64_t float_regfile_reads = stat->fpRFReads;
    uint64_t float_regfile_writes = stat->fpRFWrites;

    out_file << "int_regfile_reads=" << int_regfile_reads << "\n";
    out_file << "int_regfile_writes=" << int_regfile_writes << "\n";
    out_file << "float_regfile_reads=" << float_regfile_reads << "\n";
    out_file << "float_regfile_writes=" << float_regfile_writes << "\n";

    out_file << "function_calls=" << function_calls << "\n";

    const std::set<uint64_t> ialu_opcodes = {13, 15, 34, 53, 48, 47, 25, 26, 27, 28, 29, 30};
    const std::set<uint64_t> int_mult_opcodes = {17, 19, 20, 23, 22};

    uint64_t fpu_accesses = fp_instructions;
    uint64_t ialu_accesses = 0;
    uint64_t mul_accesses = 0;

    for (auto item : stat->insnCountMap)
    {
        llvm::Instruction *insn = item.first;
        uint64_t count = item.second;

        uint32_t opcode = insn->getOpcode();
        if (ialu_opcodes.count(opcode))
        {
            ialu_accesses += count;
        }
        else if (int_mult_opcodes.count(opcode))
        {
            mul_accesses += count;
        }
    }

    out_file << "ialu_accesses=" << ialu_accesses << "\n";
    out_file << "fpu_accesses=" << fpu_accesses << "\n";
    out_file << "mul_accesses=" << mul_accesses << "\n";

    uint64_t l1_read_accesses = l1c->read_accesses;
    uint64_t l1_write_accesses = l1c->write_accesses;
    uint64_t l1_read_misses = l1c->read_misses;
    uint64_t l1_write_misses = l1c->write_misses;

    uint64_t l2_read_accesses = l2c->read_accesses;
    uint64_t l2_write_accesses = l2c->write_accesses;
    uint64_t l2_read_misses = l2c->read_misses;
    uint64_t l2_write_misses = l2c->write_misses;

    uint64_t l3_read_accesses = l2c->nextCache->read_accesses;
    uint64_t l3_write_accesses = l2c->nextCache->write_accesses;
    uint64_t l3_read_misses = l2c->nextCache->read_misses;
    uint64_t l3_write_misses = l2c->nextCache->write_misses;

    out_file << "l1_read_accesses=" << l1_read_accesses << "\n";
    out_file << "l1_write_accesses=" << l1_write_accesses << "\n";
    out_file << "l1_read_misses=" << l1_read_misses << "\n";
    out_file << "l1_write_misses=" << l1_write_misses << "\n";

    out_file << "l2_read_accesses=" << l2_read_accesses << "\n";
    out_file << "l2_write_accesses=" << l2_write_accesses << "\n";
    out_file << "l2_read_misses=" << l2_read_misses << "\n";
    out_file << "l2_write_misses=" << l2_write_misses << "\n";

    out_file << "l3_read_accesses=" << l3_read_accesses << "\n";
    out_file << "l3_write_accesses=" << l3_write_accesses << "\n";
    out_file << "l3_read_misses=" << l3_read_misses << "\n";
    out_file << "l3_write_misses=" << l3_write_misses << "\n";

    double custom_power = 0.;
    double custom_area = 0.;
    uint64_t total_custom_insn_num = 0;
    for (auto item : stat->customInsnCountMap)
    {
        uint32_t opcode = item.first;
        uint64_t count = item.second;

        double power = stat->customInsnPowerMap[opcode];
        double area = stat->customInsnAreaMap[opcode];
        custom_power += count * power;
        custom_area += area;
        total_custom_insn_num += count;
    }

    out_file << "total_custom_insn_num=" << total_custom_insn_num << "\n";
    out_file << "Custom_power=" << custom_power << "\n";
    out_file << "Custom_area=" << custom_area << "\n";

    out_file.close();
}

void Core::printStats()
{
    std::ofstream out_file("stat.txt");
    std::cout << "offloadNumber=" << offloadNumber << "\n";
    std::cout << "rollbackNumber=" << rollbackNumber << "\n";

    std::cout << "totalFetch=" << totalFetch << "\n";
    uint64_t totalCommit = window->getTotalCommit();
    std::cout << "totalCommit=" << totalCommit << "\n";
    std::cout << "totalCycle=" << cycles << "\n";

    // top level
    uint64_t slots = cycles * window->windowSize;
    double retiring = totalCommit * 1.0 / slots;

    std::cout << "memordering=" << window->rb_flush << " mispred=" << window->mispredictNum << "\n";
    uint64_t recoverBubbles = (window->rb_flush + window->mispredictNum) * flushPenalty;
    double bad_speculation = (recoverBubbles * window->windowSize) * 1.0 / slots;

    std::cout << "==== top-down ====\n";
    std::cout << "retiring=" << retiring << "\n";
    std::cout << "bad_speculation=" << bad_speculation << "\n";
    std::cout << "backend=" << 1 - retiring - bad_speculation << "\n";

    double ipc = totalCommit * 1.0 / cycles;
    uint64_t few_threshold = ipc > 1.8 ? cycles_ge_3_uop_exec : cycles_ge_2_uop_exec;

    // we have no store buffer, so stall_sb is always 0
    uint64_t backend_bound_cycles = stalls_total + cycles_ge_1_uop_exec - few_threshold - rs_empty_cycles + stall_sb;

    std::cout << "ipc = " << ipc << "\n";
    std::cout << "cycles_ge_1_uop_exec = " << cycles_ge_1_uop_exec << "\n";
    std::cout << "cycles_ge_2_uop_exec = " << cycles_ge_2_uop_exec << "\n";
    std::cout << "cycles_ge_3_uop_exec = " << cycles_ge_3_uop_exec << "\n";
    std::cout << "few_threshold = " << few_threshold << "\n";
    std::cout << "stalls_total = " << stalls_total << "\n";
    std::cout << "stall_sb = " << stall_sb << "\n";
    std::cout << "rs_empty_cycles = " << rs_empty_cycles << "\n";
    std::cout << "backend_bound = " << backend_bound_cycles << "\n";
    std::cout << "stalls_mem_any = " << stalls_mem_any << "\n";
    std::cout << "flushN=" << window->rb_flush << " flushedINsn=" << flushedInsnNumber << "\n";

    out_file << "offloadNumber=" << offloadNumber << "\n";
    out_file << "rollbackNumber=" << rollbackNumber << "\n";
    out_file << "accCommit=" << acc->accumulateCommit << "\n";
    out_file << "accCycle=" << temp_acc_cycle << "\n";

    out_file << "ipc=" << ipc << "\n";
    out_file << "cycles_ge_1_uop_exec=" << cycles_ge_1_uop_exec << "\n";
    out_file << "cycles_ge_2_uop_exec=" << cycles_ge_2_uop_exec << "\n";
    out_file << "cycles_ge_3_uop_exec=" << cycles_ge_3_uop_exec << "\n";
    out_file << "few_threshold=" << few_threshold << "\n";
    out_file << "stalls_total=" << stalls_total << "\n";
    out_file << "stall_sb=" << stall_sb << "\n";
    out_file << "rs_empty_cycles=" << rs_empty_cycles << "\n";
    out_file << "backend_bound=" << backend_bound_cycles << "\n";
    out_file << "stalls_mem_any=" << stalls_mem_any << "\n";

    double mem_bound = (stalls_mem_any + stall_sb) * 1.0 / backend_bound_cycles;
    double core_bound = 1.0 - mem_bound;
    double backend = 1 - retiring - bad_speculation;
    std::cout << "======== " << 0. << ", " << bad_speculation << ", " << backend << " ," << retiring << ", " << mem_bound << ", " << core_bound << "\n";

    std::cout << "mem_bound=" << mem_bound << "\n";
    std::cout << "core_bound=" << core_bound << "\n";
    std::cout << "l1 DCache hit=" << l1c->getHitRate() << "\n";
    std::cout << "l1 ICache hit=" << l1i->getHitRate() << "\n";
    std::cout << "l2 hit=" << l2c->getHitRate() << "\n";
    // std::cout << "l3 hit=" << llc->getHitRate() << "\n";
    std::cout << "l1 port stall=" << l1c->portStall << "\n";
    std::cout << "l2 port stall=" << l2c->portStall << "\n";
    // std::cout << "l3 port stall=" << llc->portStall << "\n";
    std::cout << "bp mispredct num=" << window->mispredictNum << " total=" << window->totalPredictNum << "\n";
    std::cout << "BP Acc=" << 1. - (window->mispredictNum * 1.0 / window->totalPredictNum) << "\n";

    out_file << "mem_bound=" << mem_bound << "\n";
    out_file << "core_bound=" << core_bound << "\n";
    out_file << "l1_hit=" << l1c->getHitRate() << "\n";
    out_file << "l2_hit=" << l2c->getHitRate() << "\n";
    // out_file << "l3_hit=" << llc->getHitRate() << "\n";
    out_file << "l1_port_stall=" << l1c->portStall << "\n";
    out_file << "l2_port_stall=" << l2c->portStall << "\n";
    // out_file << "l3_port_stall=" << llc->portStall << "\n";
    out_file << "BP_Acc=" << 1. - (window->mispredictNum * 1.0 / window->totalPredictNum) << "\n";

    std::cout << "static insn szie=" << stat->insnCountMap.size() << "\n";

    const std::set<uint64_t> int_adder_opcodes = {13, 15, 34, 53, 48, 47};
    const std::set<uint64_t> bitwise_opcodes = {25, 26, 27, 28, 29, 30};
    const std::set<uint64_t> int_mult_opcodes = {17, 19, 20, 23, 22};
    const std::set<uint64_t> fp_mult_opcodes = {18, 21, 24};
    const std::set<uint64_t> fp_adder_opcodes = {14, 16, 54, 46, 42, 41, 45, 43};

    double int_adder_power = 0.;
    double int_mult_power = 0.;
    double fp_adder_power = 0.;
    double fp_mult_power = 0.;
    double bitwise_power = 0.;
    double reg_power = stat->totalWrite * 32 * (pam->fus[1]->internal_power + pam->fus[1]->switch_power);

    double int_adder_area = pam->fus[7]->fu_limit * pam->fus[7]->area;
    double int_mult_area = pam->fus[6]->fu_limit * pam->fus[6]->area;
    double fp_adder_area = pam->fus[3]->fu_limit * pam->fus[3]->area;
    double fp_mult_area = pam->fus[0]->fu_limit * pam->fus[0]->area;
    double bitwise_area = pam->fus[2]->fu_limit * pam->fus[2]->area;
    double reg_area = pam->fus[1]->fu_limit * pam->fus[1]->area * 32;

    for (auto item : stat->insnCountMap)
    {
        llvm::Instruction *insn = item.first;
        uint64_t count = item.second;

        uint32_t opcode = insn->getOpcode();
        HWFunctionalUnit *hwfu = pam->getHWFuncUnit(opcode);
        if (hwfu)
        {
            if (int_adder_opcodes.count(opcode))
            {
                int_adder_power += count * (hwfu->internal_power + hwfu->switch_power);
            }
            else if (int_mult_opcodes.count(opcode))
            {
                int_mult_power += count * (hwfu->internal_power + hwfu->switch_power);
            }
            else if (fp_adder_opcodes.count(opcode))
            {
                fp_adder_power += count * (hwfu->internal_power + hwfu->switch_power);
            }
            else if (fp_mult_opcodes.count(opcode))
            {
                fp_mult_power += count * (hwfu->internal_power + hwfu->switch_power);
            }
            else if (bitwise_opcodes.count(opcode))
            {
                bitwise_power += count * (hwfu->internal_power + hwfu->switch_power);
            }
        }
        else
        {
            // std::cout << "not concern, opcode=" << item.first->getOpcode() << " opname=" << item.first->getOpcodeName() << "\n";
        }
    }

    double custom_power = 0.;
    double custom_area = 0.;
    uint64_t total_custom_insn_num = 0;
    for (auto item : stat->customInsnCountMap)
    {
        uint32_t opcode = item.first;
        uint64_t count = item.second;

        double power = stat->customInsnPowerMap[opcode];
        double area = stat->customInsnAreaMap[opcode];
        custom_power += count * power;
        custom_area += area;
        total_custom_insn_num += count;
        out_file << to_string(opcode) + "=" << count << "\n";
    }

    if (dyser)
    {
        for (auto item1 : dyserUnits)
        {
            auto dyserUnit = item1.second;

            for (auto item : dyserUnit->getInsnCountMap())
            {
                uint32_t opcode = item.first;
                uint64_t count = item.second;
                out_file << "dyser_" + to_string(opcode) + "=" << count << "\n";
            }
        }
    }

    out_file << "slots=" << slots << "\n";
    out_file << "stalls_mem_any=" << stalls_mem_any << "\n";
    out_file << "total_custom_insn_num=" << total_custom_insn_num << "\n";
    out_file << "packing_func_num=" << stat->packing_func_num << "\n";
    out_file << "extract_insn_num=" << stat->extract_insn_num << "\n";

    std::cout << "Custom power=" << custom_power << " area=" << custom_area << "\n";
    std::cout << "Int adder power=" << int_adder_power << " area=" << int_adder_area << "\n";
    std::cout << "Int mult power=" << int_mult_power << " area=" << int_mult_area << "\n";
    std::cout << "Fp adder power=" << fp_adder_power << " area=" << fp_adder_area << "\n";
    std::cout << "Fp mult power=" << fp_mult_power << " area=" << fp_mult_area << "\n";
    std::cout << "Bitwise power=" << bitwise_power << " area=" << bitwise_area << "\n";
    std::cout << "Reg power=" << reg_power << " area=" << reg_area << "\n";

    double total_power = int_adder_power + int_mult_power + fp_adder_power + fp_mult_power + bitwise_power + reg_power + custom_power;
    double total_area = int_adder_area + int_mult_area + fp_adder_area + fp_mult_area + bitwise_area + reg_area + custom_area;
    std::cout << "total power=" << total_power << " area=" << total_area << "\n";

    out_file << "stalls_custom_insn=" << stat->cstm_insn_stall << "\n";
    out_file << "packing_insn_stall=" << stat->packing_insn_stall << "\n";
    out_file << "ext_insn_stall=" << stat->ext_insn_stall << "\n";

    out_file << "stalls_custom_insn_ratio=" << stat->cstm_insn_stall * 1.0 / cycles << "\n";
    out_file << "packing_insn_stall_ratio=" << stat->packing_insn_stall * 1.0 / cycles << "\n";
    out_file << "ext_insn_stall_ratio=" << stat->ext_insn_stall * 1.0 / cycles << "\n";

    out_file << "totalFetch=" << totalFetch << "\n";
    out_file << "totalCommit=" << window->getTotalCommit() << "\n";
    out_file << "totalCycle=" << cycles << "\n";

    out_file << "Custom_power=" << custom_power << "\n";
    out_file << "Custom_area=" << custom_area << "\n";
    out_file << "Int_adder_power=" << int_adder_power << "\n";
    out_file << "Int_adder_area=" << int_adder_area << "\n";
    out_file << "Int_mult_power=" << int_mult_power << "\n";
    out_file << "Int_mult_area=" << int_mult_area << "\n";
    out_file << "Fp_adder_power=" << fp_adder_power << "\n";
    out_file << "Fp_adder_area=" << fp_adder_area << "\n";
    out_file << "Fp_mult_power=" << fp_mult_power << "\n";
    out_file << "Fp_mult_area=" << fp_mult_area << "\n";
    out_file << "Bitwise_power=" << bitwise_power << "\n";
    out_file << "Bitwise_area=" << bitwise_area << "\n";
    out_file << "Reg_power=" << reg_power << "\n";
    out_file << "Reg_area=" << reg_area << "\n";
    out_file << "Total_power=" << total_power << "\n";
    out_file << "Total_area=" << total_area << "\n";
    out_file.close();
}

void Core::fetch()
{
    // if (flushStall)
    //     return;

    // if flush we must execute flushed insns before any other insns
    if (flushedInsns.size())
    {
        processedOps.insert(processedOps.begin(), flushedInsns.begin(), flushedInsns.end());
        flushedInsns.clear();
    }

    if (insnQueue.size() >= insnQueueMaxSize)
    {
        return;
    }

    // FTQ size = 32
    if (FTQ.size() <= 32)
    {
        // convert static insn to dynamic insns
        auto ops = preprocess();

        // TODO: fix this, add to FTQ with package size is not right
        for (int i = 0; i < ops.size(); i++)
        {
            if (i % ifu->getPackageSize() == 0)
            {
                FTQ.push_back(ops[i]);
            }

            processedOps.push_back(ops[i]);
        }
    }

    ifu->IF1(processedOps, insnQueue);

    auto tempPrevBBTerminator = FTQ[0]->prevBBTerminator;
    if (tempPrevBBTerminator && FTQ[0]->waitingForBrDecision)
    {
        if (tempPrevBBTerminator->mispredict)
        {
            // cout << FTQ[0]->getOpName() << " " << tempPrevBBTerminator->getOpName() << "\n";
            // cout << "     ===" << insn_to_string_1(tempPrevBBTerminator->getStaticInsn()->getLLVMInsn()) << "\n";
            // cout << "     ===" << insn_to_string_1(FTQ[0]->getStaticInsn()->getLLVMInsn()) << "\n";

            assert(tempPrevBBTerminator->status != DynOpStatus::SLEEP);
            
            // if mispredict, waiting for branch decision
            // stop fetch
            if (tempPrevBBTerminator->status < DynOpStatus::FINISHED)
            {
                return;
            }
            // flush penalty due to mispredict
            else if (tempPrevBBTerminator->status >= DynOpStatus::FINISHED)
            {
                // in case IF0 is stalled
                FTQ[0]->waitingForBrDecision = false;
                rb_flushedN += 1;

                flushStallCycle = flushPenalty;
                flushStall = true;
                flushN++;
            }
        }
    }

    if (flushStall)
        return;

    ifu->IF0(FTQ);
}

// perfect branch prediction
std::vector<std::shared_ptr<DynamicOperation>> Core::preprocess()
{
    std::vector<std::shared_ptr<DynamicOperation>> ops;

    if (processedOps.size() >= insnQueueMaxSize)
    {
        return ops;
    }

    fetchOpBuffer.clear();
    if (fetchForRet)
    {
        // main ret if size == 0
        if (returnStack.size() != 0)
        {
            auto retOps = returnStack.top();
            fetchOpBuffer = retOps->ops;
            bool isInvoke = retOps->isInvoke;
            if (isInvoke)
            {
                if (!task->at_end())
                {
                    string bbname = readBBName();
                }
            }
            returnStack.pop();

            previousBB = prevBBStack.top();
            prevBBStack.pop();

            prevBBTerminator = prevBBTermStack.top();
            prevBBTermStack.pop();
        }
        else
        {
            // in simpoint, the simulator may not know
            // return address of a ret insn, so we fetch
            // bb according to trace
            if (simpoint)
            {
                // cout << "ret fetch bb\n";
                targetBB = fetchTargetBB();
                // assert(targetBB);
                if (targetBB)
                {
                    fetchOpBuffer = scheduleBB(targetBB);
                }
                else
                {
                    cout << " warning:  ret fetch empty bb !!!\n";
                }
            }
        }
    }
    else
    {
        if (targetBB)
        {
            fetchOpBuffer = scheduleBB(targetBB);
        }
    }
    // targetBB = nullptr;
    fetchForRet = false;
    bool newBBScheduled = false;

    if (abnormalExit)
    {
        targetBB = nullptr;
        fetchOpBuffer.clear();
        return ops;
    }

    if (prevBBTerminator)
    {
        if (prevBBTerminator->isConditional())
        {
            fetchOpBuffer[0]->prevBBTerminator = prevBBTerminator;
        }
    }

    // add to FTQ according to IFU->getPackageSize
    for (int i = 0; i < fetchOpBuffer.size(); i++)
    {
        auto insn = fetchOpBuffer[i];
        insn->updateDynID(globalDynID);
        globalDynID++;
        ops.push_back(insn);
        // insnQueue.push_back(insn);

        totalFetch++;

        // cout << insn->getOpName() << "\n";
        // cout << insn_to_string_1(insn->getStaticInsn()->getLLVMInsn()) << "\n";

        if (insn->isBr())
        {
            // std::cout << "br inst\n";
            auto br = std::dynamic_pointer_cast<BrDynamicOperation>(insn);
            int cond = true;

            llvm::BranchInst *llvm_br = llvm::dyn_cast<llvm::BranchInst>(insn->getStaticInsn()->getLLVMInsn());
            if (llvm_br->isConditional())
            {
                if (task->at_end())
                {
                    abnormalExit = true;
                    break;
                }
                // the moment switching from acc to host
                if (switching)
                {
                    // cout << "switching... rollback=" << rollback << "\n";
                    // clear flag
                    switching = false;
                    if (!speculative)
                    {
                        cond = readBrCondition();
                        br->mispredict = !bp->predict(insn->getUID(), cond);
                        // cout << cond << "\n";
                    }
                    else
                    {
                        // if speculatively executing,
                        // manually jump to offload.true (rollback == 0)
                        // or offload.false (rollback == 1)
                        if (rollback)
                        {
                            rollbackNumber++;
                            task->resetReadingPos();
                            cond = false;
                        }

                        // clear flag
                        rollback = false;
                    }
                }
                else
                {
                    cond = readBrCondition();
                    br->mispredict = !bp->predict(insn->getUID(), cond);
                    // cout << cond << "\n";
                }
            }

            br->setCondition(cond);
            newBBScheduled = true;
            previousBB = targetBB;
            targetBB = fetchTargetBB(br);

            prevBBTerminator = insn;

            // std::cout << "branch target BB name=" << targetBB->getBBName() << "\n";
            break;
        }
        else if (insn->isCall())
        {
            // std::cout << "call inst\n";
            auto call = std::dynamic_pointer_cast<CallDynamicOperation>(insn);
            previousBB = targetBB;

            prevBBTerminator = insn;

            // we need to save rollback point here
            // because fetchTargetBB will read data in simpoint

            if (speculative)
            {
                task->saveRollbackPos();
            }
            targetBB = fetchTargetBB(call);

            if (targetBB)
            {
                newBBScheduled = true;
                // std::cout << "function target BB name=" << targetBB->getBBName() << "\n";
                std::vector<std::shared_ptr<DynamicOperation>> returnBB(fetchOpBuffer.begin() + i + 1, fetchOpBuffer.begin() + fetchOpBuffer.size());
                returnStack.push(new RetOps(returnBB));
                prevBBStack.push(previousBB);

                prevBBTermStack.push(prevBBTerminator);

                fetchOpBuffer.clear();

                if (call->getCalledFunc() && call->getCalledFunc()->getName().str() == offload_func_name)
                {
                    // cout << "offload function cycle=" << cycles << "\n";

                    // stop fetching
                    newBBScheduled = false;

                    call->targetBB = targetBB;
                    call->globalDynID = globalDynID;
                    call->offloaded = false;
                }

                break;
            }
        }
        else if (insn->isInvoke())
        {
            auto invoke = std::dynamic_pointer_cast<InvokeDynamicOperation>(insn);
            previousBB = targetBB;
            prevBBTerminator = insn;

            // this get entry bb of called function
            targetBB = fetchTargetBB(invoke);

            // get normal dest bb
            llvm::Value *destValue = invoke->getDestValue();
            auto normalDestBB = task->fetchBB(destValue);
            newBBScheduled = true;

            if (targetBB)
            {
                // only branch to normal dest
                auto normalDestBBOps = scheduleBB(normalDestBB);

                returnStack.push(new RetOps(normalDestBBOps, true));
                prevBBStack.push(previousBB);

                prevBBTermStack.push(prevBBTerminator);

                break;
            }
            else // this function is declartion then invoke is like a br
            {
                if (!task->at_end())
                {
                    if (simpoint)
                    {
                        string bbname = readBBName();
                        // cout << bbname << "\n";
                    }
                }
                targetBB = normalDestBB;
                break;
            }
        }
        else if (insn->isRet())
        {
            // std::cout << "ret inst" << "\n";
            fetchForRet = true;
            break;
        }
        else if (insn->isSwitch())
        {
            if (task->at_end())
            {
                abnormalExit = true;
                break;
            }
            // std::cout << "switch inst\n";
            auto sw = std::dynamic_pointer_cast<SwitchDynamicOperation>(insn);
            int cond = readSwitchCondition();
            sw->setCondition(cond);
            // std::cout << std::hex << cond << "\n";
            // std::cout << std::dec;
            newBBScheduled = true;
            previousBB = targetBB;
            prevBBTerminator = insn;
            targetBB = fetchTargetBB(sw);
            // std::cout << "switch target BB name=" << targetBB->getBBName() << "\n";
            break;
        }
        else if (insn->isLoad())
        {
            if (task->at_end())
            {
                abnormalExit = true;
                break;
            }
            // std::cout << "load inst\n";
            auto load = std::dynamic_pointer_cast<LoadDynamicOperation>(insn);
            uint64_t addr = readAddr();
            load->setAddr(addr);
            // std::cout << std::hex << addr << "\n";
            // std::cout << std::dec;
        }
        else if (insn->isStore())
        {
            if (task->at_end())
            {
                abnormalExit = true;
                break;
            }
            // std::cout << "store inst\n";
            auto store = std::dynamic_pointer_cast<StoreDynamicOperation>(insn);
            uint64_t addr = readAddr();
            store->setAddr(addr);
            // std::cout << std::hex << addr << "\n";
            // std::cout << std::dec;
        }
        else if (insn->isPhi())
        {
            auto phi = std::dynamic_pointer_cast<PhiDynamicOperation>(insn);
            if (previousBB)
                phi->setPreviousBB(previousBB);
        }
        else
        {
            // std::cout << "arith inst\n";
        }
    }

    if (!newBBScheduled || abnormalExit)
        targetBB = nullptr;

    return ops;
}

void Core::insnToIssueQueues(std::vector<std::shared_ptr<DynamicOperation>> &insnToIssue)
{
    for (auto insn : insnToIssue)
    {
        issueQueue.push_back(insn);

        // uidDynOpMap mapping static uid to most recent dynop
        // because we just want to build dependency to last instance (dynop) of static insn
        uidDynOpMap[insn->getUID()] = insn;

        auto deps = insn->getStaticDepUIDs();
        for (auto dep : deps)
        {
            // assert(uidDynOpMap.find(dep) != uidDynOpMap.end());
            if (uidDynOpMap.find(dep) != uidDynOpMap.end())
            {
                auto dynOp = uidDynOpMap[dep];
                insn->addDynDeps(dynOp->getDynID());
            }
        }
    }
}

void Core::issue()
{
    // if (flushStall)
    //     return;

    auto readyInsns = issueUnit->issue(issueQueue, window, ports, lsu, cycles, rou);
    if (window->ifROBEmpty())
    {
        rs_empty_cycles++;
    }

    if (readyInsns.size() == 0)
    {
        if (lsu->getExecutingLoads() > 0)
        {
            stalls_mem_any++;
        }
        stalls_total++;

        int cstm_on_the_fly = 0;
        for (uint64_t i = 0; i < inFlightInsn.size(); i++)
        {
            if (inFlightInsn[i]->isEncapsulateFunc && inFlightInsn[i]->status == DynOpStatus::EXECUTING)
            {
                cstm_on_the_fly++;
            }
        }

        if (cstm_on_the_fly > 0 && !lsu->getExecutingLoads())
        {
            stat->cstm_insn_stall++;
        }
    }

    // issue insn but not other insns on the fly
    if (readyInsns.size() && readyInsns[0]->isPackingFunc)
    {
        int pk_on_the_fly = 0;
        for (uint64_t i = 0; i < inFlightInsn.size(); i++)
        {
            if (inFlightInsn[i]->status == DynOpStatus::EXECUTING)
            {
                pk_on_the_fly++;
            }
        }

        if (pk_on_the_fly == 0)
        {
            stat->packing_insn_stall++;
        }
    }

    if (readyInsns.size() && readyInsns[0]->getOpName() == "extractvalue")
    {
        int ext_on_the_fly = 0;
        for (uint64_t i = 0; i < inFlightInsn.size(); i++)
        {
            if (inFlightInsn[i]->status == DynOpStatus::EXECUTING)
            {
                ext_on_the_fly++;
            }
        }

        if (ext_on_the_fly == 0)
        {
            stat->ext_insn_stall++;
        }
    }

    if (readyInsns.size() >= 3)
    {
        cycles_ge_3_uop_exec++;
    }

    if (readyInsns.size() >= 2)
    {
        cycles_ge_2_uop_exec++;
    }

    if (readyInsns.size() >= 1)
    {
        cycles_ge_1_uop_exec++;
    }
    // std::cout << "cycle=" << cycles << " readyInsns.sze()" << readyInsns.size() << "\n";
    inFlightInsn.insert(inFlightInsn.end(), readyInsns.begin(), readyInsns.end());
}

void Core::dispatch()
{
    // if (flushStall)
    //     return;

    std::vector<std::shared_ptr<DynamicOperation>> dispatchedInsns;
    uint64_t ws = window->getWindowSize();
    for (int i = 0; i < ws; i++)
    {
        if (insnQueue.size() == 0)
            break;

        auto insn = insnQueue[0];

        // cout << "dispatch " << window->canDispatch(insn) << " " << window->ifROBFull() << " " << lsu->canDispatch(insn) << "\n";

        // check if issuequeue, rob, and ld/st queue is full
        if (window->canDispatch(insn) && !window->ifROBFull() && lsu->canDispatch(insn))
        {
            dispatchedInsns.push_back(insn);
            window->incrementIssueQueueUsed();

            assert(insn->status == DynOpStatus::INIT);
            insn->updateStatus();

            // to rob
            window->addInstruction(insn);
            // allocate entry in lsu
            lsu->addInstruction(insn);

            rou->addInstruction(insn);

            insnQueue.erase(insnQueue.begin());
            insn->D = cycles;
        }
        else
        {
            break;
        }
    }
    insnToIssueQueues(dispatchedInsns);
}

void Core::writeBack()
{
    // if (flushStall)
    //     return;

    lsu->wb();
    for (uint64_t i = 0; i < inFlightInsn.size(); i++)
    {
        auto insn = inFlightInsn[i];
        // some insns may be already in FINISHED
        // such load and store is set finsihed in lsu->process
        // and insns complete but no commit
        if (insn->status != DynOpStatus::FINISHED && insn->finish())
        {
            insn->E = cycles;
            insn->status = DynOpStatus::FINISHED;
        }

        // cstm insn free fu at stage cycle
        // if (insn->freeFUEarly())
        // {
        //     issueUnit->freeFU(insn->usingFuIDs, insn->usingPorts);
        //     insn->freeFU();
        // }

        if (insn->status == DynOpStatus::FINISHED)
        {
            issueUnit->freeFU(insn->usingFuIDs, insn->usingPorts);
            insn->freeFU();
        }
    }
}

void Core::IE()
{
    issue();
    execute();
}

void Core::execute()
{
    // if (flushStall)
    //     return;

    for (uint64_t i = 0; i < inFlightInsn.size(); i++)
    {
        auto insn = inFlightInsn[i];

        // offload
        if (insn->isCall())
        {
            auto call = std::dynamic_pointer_cast<CallDynamicOperation>(insn);
            assert(call);
            if (call->getCalledFunc() && !offloading && !call->offloaded && call->getCalledFunc()->getName().str() == offload_func_name)
            {
                // cout << "offloading\n";
                offload(acc, call->targetBB, call->globalDynID);
                offloading = true;
                rollback = false;

                call->offloaded = true;

                offloadNumber++;
            }
        }

        if (insn->status != DynOpStatus::FINISHED)
        {
            insn->execute();

            if (insn->status == DynOpStatus::SLEEP)
            {
                issueUnit->freeFU(insn->usingFuIDs, insn->usingPorts);
                insn->freeFU();
            }

            if (dyser && insn->isPackingFunc)
            {
                if (insn->offset != -1)
                {
                    assert(dyserUnits.count(insn->dyserUnitID));
                    auto dyserUnit = dyserUnits[insn->dyserUnitID];
                    // cout<< "notify "<<" dynid="<<insn->getDynID()<<" name="<<insn->getOpName() << insn->offset << " " << insn->stride<<" "<<cycles << "\n";
                    dyserUnit->notify(insn->offset, insn->stride);
                    // avoid duplicate notify
                    insn->offset = -1;
                }
            }

            if (dyser && insn->isExtractFunc)
            {
                assert(insn->offset != -1);
                assert(dyserUnits.count(insn->dyserUnitID));
                auto dyserUnit = dyserUnits[insn->dyserUnitID];
                if (dyserUnit->getData(insn->offset))
                {
                    // cout<< "extract "<<" dynid="<<insn->getDynID()<<" name="<<insn->getOpName() << insn->offset << " " << insn->stride<<" "<<cycles << "\n";
                    insn->setFinish(true);
                }
            }
        }
    }
    lsu->process();
}

bool compareInsn(const std::shared_ptr<DynamicOperation> a, const std::shared_ptr<DynamicOperation> b)
{
    return a->getDynID() < b->getDynID();
}

void Core::commit()
{
    // if (flushStall)
    //     return;

    uint64_t flushDynID = 0;
    // flush after insn (exclusive) of flushDynID because of memory disambiguation

    window->retireInstruction(stat, lsu, flushDynID, cycles, funcCycles);

    // remove ops that has finished and commited
    for (auto it = inFlightInsn.begin(); it != inFlightInsn.end();)
    {
        std::shared_ptr<DynamicOperation> insn = *it;
        if (insn->status == DynOpStatus::COMMITED)
        {
            it = inFlightInsn.erase(it);
        }
        else
        {
            it++;
        }
    }
}

void Core::run()
{

    l2c->process();
    l1c->process();
    l1i->process();

    commit();
    writeBack();
    // IE();

    execute();
    // when core flush, acc should be still ticking
    acc->tick();
    for (auto item : dyserUnits)
    {
        item.second->tick();
    }
    // dyserUnit->tick();

    issue();
    dispatch();

    fetch();

    // insert bubble for flush
    if (flushStall)
    {
        // flush the entire cycle
        if (flushStallCycle > 0)
        {
            flushStallCycle--;
        }
        assert(flushStallCycle >= 0);
        flushStall = false;
    }

    cycles++;

    if (
        processedOps.size() == 0 &&
        insnQueue.size() == 0 &&
        issueQueue.size() == 0 &&
        inFlightInsn.size() == 0 &&
        window->ifROBEmpty() &&
        flushedInsns.size() == 0 &&
        !offloading)
        running = false;

    return;
}

void Core::offload(std::shared_ptr<Accelerator> acc, std::shared_ptr<SIM::BasicBlock> targetBB, uint64_t gid)
{
    acc->switchContext(targetBB, gid);
}

void Core::switchContext(uint64_t gid, bool failure, bool abnormal)
{
    if (!failure)
    {
        globalDynID = gid;

        window->totalCommit += acc->totalCommit;
        totalFetch += acc->totalFetch;

        temp_acc_cycle = acc->temp_acc_cycle;
    }

    offloading = false;
    rollback = failure;
    switching = true;
    abnormalExit = abnormal;

    assert(returnStack.size());
    fetchForRet = true;
}

uint64_t Core::getTotalCommit()
{
    if (acc)
    {
        return window->totalCommit + acc->totalCommit;
    }

    return window->totalCommit;
}