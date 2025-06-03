#include <iostream>
#include <vector>
#include <string>
#include <map>

#include <iostream>
#include "../include/task/instruction.h"
#include "../include/task/basic_block.h"
#include "../include/task/function.h"
#include "../include/task/task.h"
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <chrono>
#include <regex>

using namespace std;

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

#ifdef _OPENMP
#include <omp.h>
// getting maximum number of threads available
static const int MAX_THREADS = (omp_get_thread_num() == 0) ? omp_get_max_threads() : omp_get_thread_num();
#endif

int main(int argc, char **argv)
{
    auto start = chrono::high_resolution_clock::now();
    string ir_file = string(argv[1]);
    string start_bb_name = string(argv[2]);
    string funcname_file = string(argv[3]);

    set<string> funcNames;
    if (funcname_file != "None")
        funcNames = readFuncNames(funcname_file);

    bool funcUseWhiteList = funcNames.size() > 0;

    cout << "funcUseWhiteList=" << funcUseWhiteList << " size=" << funcNames.size() << "\n";

    llvm::StringRef file = ir_file;
    unique_ptr<llvm::LLVMContext> context(new llvm::LLVMContext());
    unique_ptr<llvm::SMDiagnostic> error(new llvm::SMDiagnostic());
    unique_ptr<llvm::Module> m;

    m = llvm::parseIRFile(file, *error, *context);
    unordered_map<llvm::Value *, shared_ptr<SIM::Value>> value_map;
    vector<shared_ptr<SIM::Function>> func_list;

    unordered_map<uint64_t, llvm::Instruction *> uid_insn_map;

    int funcNum = 0;
    int bbNum = 0;
    uint64_t uid = 0;
    for (auto &func : *m)
    {
        string funcName = func.getName().str();
        // decl and intrin func will never be fetched ?
        if (funcUseWhiteList && !funcNames.count(funcName))
            continue;

        shared_ptr<SIM::Function> sfunc = make_shared<SIM::Function>(uid++, &func);
        value_map.insert(make_pair<llvm::Value *, shared_ptr<SIM::Value>>(&func, sfunc));
        func_list.push_back(sfunc);

        for (auto &bb : func)
        {
            shared_ptr<SIM::BasicBlock> sbb = make_shared<SIM::BasicBlock>(uid++, &bb);
            value_map.insert(make_pair<llvm::Value *, shared_ptr<SIM::Value>>(&bb, sbb));

            for (auto &insn : bb)
            {
                shared_ptr<SIM::Instruction> s_insn = make_shared<SIM::Instruction>(uid++, &insn);
                value_map.insert(make_pair<llvm::Value *, shared_ptr<SIM::Value>>(&insn, s_insn));
                uid_insn_map[s_insn->getUID()] = &insn;
            }

            bbNum++;
        }

        funcNum++;
        if (funcNum % 100 == 0)
        {
            cout << "read " << funcNum << " functions" << "\n";
        }
    }

    cout << "total func=" << funcNum << " bb=" << bbNum << " vmap size=" << value_map.size() << " uid mapsize=" << uid_insn_map.size() << "\n";

    vector<vector<string>> func_data(func_list.size(), vector<string>());

#ifdef _OPENMP
#pragma omp parallel for num_threads(MAX_THREADS)
#endif
    for (int i = 0; i < func_list.size(); i++)
    {
        auto sfunc = func_list[i];
        sfunc->initialize(value_map);
        auto data = sfunc->getInsnDepData();
        func_data[i] = data;

        // cout << "finish func=" << sfunc->getFuncName() << " insn size=" << data.size() << "\n";
    }

    std::ofstream out_file("init_file.data");
    for (auto data : func_data)
    {
        for (string dep : data)
        {
            out_file << dep << "\n";
        }
    }
    cout << "Initialize func finish...\n";

    out_file.close();

    return 1;
}
