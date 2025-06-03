#ifndef INCLUDE_POWER_AREA_MODEL_H
#define INCLUDE_POWER_AREA_MODEL_H

#include <vector>
#include <set>
#include "task/dyn_insn.h"

using namespace std;

class HWFunctionalUnit
{
public:
    HWFunctionalUnit() {}
    HWFunctionalUnit(double ip, double sp, double lp, double area, double limit)
        : internal_power(ip), switch_power(sp), leakage_power(lp), area(area), fu_limit(limit) {}

    double internal_power = 0.;
    double switch_power = 0.;
    double leakage_power = 0.;
    double area = 0.;

    int fu_limit = 0;
};

class PowerAreaModel
{
public:
    PowerAreaModel();
    HWFunctionalUnit *getHWFuncUnit(uint32_t opcode);
    vector<HWFunctionalUnit *> fus;

private:
    const set<uint64_t> int_adder_opcodes = {12, 13, 15, 34, 38, 53, 48, 47, 57, 39, 40};
    const set<uint64_t> bitwise_opcodes = {25, 26, 27, 28, 29, 30};
    const set<uint64_t> int_mult_opcodes = {17, 19, 20, 23, 22};
    const set<uint64_t> fp_mult_opcodes = {18, 21, 24};
    const set<uint64_t> fp_adder_opcodes = {14, 16, 54, 46, 42, 41, 45, 43, 44};
};

#endif