#include <string>
#include <map>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <set>
#include <fstream>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Support/SourceMgr.h"
#include <llvm/IRReader/IRReader.h>
#include "llvm/IR/Instructions.h"

using namespace std;
using namespace llvm;

set<string> readFuncNames(string filename)
{
    std::fstream file;
    set<string> ret;

    file.open(filename);

    std::string funcName;
    while (file >> funcName)
    {
        ret.insert(funcName);
    }

    return ret;
}

unordered_map<string, uint64_t> readBBCycles(string filename)
{
    std::fstream file;
    unordered_map<string, uint64_t> ret;

    file.open(filename);

    std::string bbName;
    uint64_t cycle;
    while (file >> bbName >> cycle)
    {
        ret[bbName] = cycle;
    }

    return ret;
}

struct BBCycle
{
    BBCycle(string _name, uint64_t _cycle) : name(_name), cycle(_cycle) {}
    string name;
    uint64_t cycle;
    float weights;
};

int main(int argc, char **argv)
{
    string ir_file = string(argv[1]);
    string funcname_file = string(argv[2]);
    string cycles_file = string(argv[3]);
    float threshold = stof(argv[4]);
    string bblist = string(argv[5]);
    string weights = string(argv[6]);

    llvm::StringRef file = ir_file;
    unique_ptr<llvm::LLVMContext> context(new llvm::LLVMContext());
    unique_ptr<llvm::SMDiagnostic> error(new llvm::SMDiagnostic());
    unique_ptr<llvm::Module> m;

    m = llvm::parseIRFile(file, *error, *context);

    set<string> functionNames = readFuncNames(funcname_file);
    unordered_map<string, uint64_t> bb_cycles = readBBCycles(cycles_file);

    vector<string> bbs;
    vector<uint64_t> cycles;

    vector<BBCycle *> items;
    uint64_t total_cycle = 0;

    for (auto &func : *m)
    {
        if (!functionNames.count(func.getName().str()))
            continue;

        for (auto &bb : func)
        {
            bool skip = false;
            for (auto &insn : bb)
            {
                if (insn.getOpcodeName() == "call")
                {
                    CallInst *call = dyn_cast<CallInst>(&insn);
                    auto func = call->getCalledFunction();
                    if (!func || (func && (!func->isDeclaration() && !func->isIntrinsic())))
                    {
                        errs() << *call << "\n";
                        skip = true;
                        break;
                    }
                }
                else if (insn.getOpcodeName() == "invoke")
                {
                    InvokeInst *invoke = dyn_cast<InvokeInst>(&insn);
                    auto func = invoke->getCalledFunction();
                    if (!func || (func && (!func->isDeclaration() && !func->isIntrinsic())))
                    {
                        errs() << *invoke << "\n";
                        skip = true;
                        break;
                    }
                }
            }
            if (!skip)
            {
                string name = bb.getName().str();
                if (bb_cycles.count(name))
                {
                    uint64_t cycle = bb_cycles[name];
                    items.push_back(new BBCycle(name, cycle));
                    total_cycle += cycle;
                }
            }
        }
    }

    std::sort(items.begin(), items.end(), [](const BBCycle *a, const BBCycle *b)
              { return a->cycle > b->cycle; });
    
    std::ofstream bblist_file(bblist);
    std::ofstream weights_file(weights);

    float w = 0;
    for (auto item : items)
    {
        item->weights = item->cycle * 1.0 / total_cycle;
        cout << "bb=" << item->name << " " << item->cycle << " " << item->weights << "\n";
        w += item->weights;

        bblist_file << item->name << "\n";
        weights_file << item->name << " " << item->weights << " " << item->weights << " " << item->cycle << "\n";

        if (w >= threshold)
            break;
    }

    bblist_file.close();
    weights_file.close();

    // std::ofstream out_file("init_file.data");
    // for (auto data : func_data)
    // {
    //     for (string dep : data)
    //     {
    //         out_file << dep << "\n";
    //     }
    // }
    // cout << "Initialize func finish...\n";

    // out_file.close();

    return 1;
}