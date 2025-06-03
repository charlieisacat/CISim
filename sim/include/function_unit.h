#ifndef INCLUDE_FU_H
#define INCLUDE_FU_H
#include "task/llvm.h"

enum FUType
{
    INVFU, // invalid fu
    INTALU,
    INTMUL,
    INTDIV,
    FPALU,
    FPMUL,
    FPDIV,
    AGU,
    BRU,
    LU, // load data
    SU, // store data
    FP,   // fp add, mul, div
    FMA,   // fp add, mul, div
    COUNT,
};

const std::map<std::string, int> strFUMap = {
    {"INTALU", FUType::INTALU},
    {"INTMUL", FUType::INTMUL},
    {"INTDIV", FUType::INTDIV},
    {"FPALU", FUType::FPALU},
    {"FPMUL", FUType::FPMUL},
    {"FPDIV", FUType::FPDIV},
    {"AGU", FUType::AGU},
    {"BRU", FUType::BRU},
    {"LU", FUType::LU},
    {"SU", FUType::SU},
    {"FMA", FUType::FMA},
    {"FP", FUType::FP}};

class FunctionalUnit
{
public:
    FunctionalUnit() {}
    FunctionalUnit(FUType _fuType) : fuType(_fuType) {}
    virtual FUType getFUType() { return fuType; }

protected:
    FUType fuType = FUType::INVFU;
};
#endif