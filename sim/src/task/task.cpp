#include "task/task.h"
#include <sstream>
#include <iostream>

void SIM::Task::initialize()
{
    trace_fs.open(trace_file);
    rollbackPos = trace_fs.tellg();
}

void SIM::Task::finish()
{
    trace_fs.close();
}

std::string SIM::Task::parse_bb_name()
{
    assert(!at_end());
    std::string hex_str = "0xFFFFFFFF";
    trace_fs >> hex_str;
    assert(hex_str != "");
    // std::cout<<"trace_fs="<<hex_str<<"\n";

    int value;
    std::stringstream ss;
    ss << std::hex << hex_str;
    ss >> value;
    return "r" + std::to_string(value);
}

bool SIM::Task::parse_br()
{
    assert(!at_end());
    bool cond = false;
    trace_fs >> cond;
    return cond;
}

int SIM::Task::parse_switch()
{
    assert(!at_end());
    std::string hex_str = "0x0";
    trace_fs >> hex_str;

    uint64_t value = 0;
    std::stringstream ss;
    ss << std::hex << hex_str;
    ss >> value;

    int cond = static_cast<int>(value);
    return cond;
}

uint64_t SIM::Task::parse_cycle()
{
    std::string hex_str = "0x0";
    trace_fs >> hex_str;

    uint64_t cycle = 0;
    std::stringstream ss;
    ss << std::hex << hex_str;
    ss >> cycle;

    return cycle;
}

uint64_t SIM::Task::parse_addr()
{
    assert(!at_end());
    std::string hex_str = "0x0";
    trace_fs >> hex_str;

    uint64_t addr = 0;
    std::stringstream ss;
    ss << std::hex << hex_str;
    ss >> addr;

    return addr;
}

std::shared_ptr<SIM::BasicBlock> SIM::Task::fetchBB(llvm::Value *llvmValue)
{
    auto mapit = vmap.find(llvmValue);
    assert(mapit != vmap.end());
    std::shared_ptr<SIM::BasicBlock> sbb = std::dynamic_pointer_cast<SIM::BasicBlock>(mapit->second);
    return sbb;
}

std::shared_ptr<SIM::Function> SIM::Task::fetchFunc(llvm::Value *llvmValue)
{
    auto mapit = vmap.find(llvmValue);
    assert(mapit != vmap.end());
    std::shared_ptr<SIM::Function> sfunc = std::dynamic_pointer_cast<SIM::Function>(mapit->second);
    return sfunc;
}