#include <iostream>
#include <string>
#include <filesystem>
#include <stack>
#include <iomanip>
namespace fs = std::filesystem;

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/MemoryBuffer.h"

#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include <map>

#include <cmath>
using namespace llvm;
using namespace std;

vector<string> split(const string &s, char delimiter)
{
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

int main(int argc, char **argv)
{
    std::string ir_path = argv[1];
    string input_file = argv[2];
    int interval = stoi(argv[3]);
    string bblist_filename = argv[4];
    string weights_filename = argv[5];
    double th = stod(argv[6]);

    LLVMContext Context;
    SMDiagnostic Err;

    std::unique_ptr<Module> M = parseIRFile(ir_path, Err, Context);

    int bb_insn = 0;
    int cstn_insn = 0;

    uint64_t uid = 0;
    std::vector<Instruction *> instructions;
    set<string> ban_set;
    for (auto &F : *M)
    {
        for (auto &BB : F)
        {
            string bbName = BB.getName().str();
            for (auto &I : BB)
            {
                // if I is call inst
                if (isa<CallInst>(&I))
                {
                    ban_set.insert(bbName);
                    break;
                }
            }
        }
    }

    ifstream fin(input_file);
    string line;
    bool found = false;
    int current_line = 0;
    while (getline(fin, line))
    {
        if (current_line == interval)
        {
            found = true;
            break;
        }
        current_line++;
    }
    fin.close();

    vector<string> items;
    istringstream iss(line);
    string item;
    while (iss >> item)
    {
        items.push_back(item);
    }

    map<int, int> bb_count_map;
    int total_count = 0;

    for (const auto &item : items)
    {
        cout << "item = " << item << "\n";
        vector<string> tokens = split(item, ':');
        int bb = stoi(tokens[1]) - 1; // Adjust for 0-based index
        int count = stoi(tokens[2]);

        if (ban_set.count("r" + to_string(bb)))
        {
            cout << "bb " << bb << " is in ban_set\n"; 
            continue;
        }

        bb_count_map[bb] = count;
        total_count += count;
    }

    vector<pair<int, int>> sorted_vec(bb_count_map.begin(), bb_count_map.end());
    std::sort(sorted_vec.begin(), sorted_vec.end(),
              [](const pair<int, int> &a, const pair<int, int> &b)
              {
                  return a.second > b.second;
              });
    cout << "sorted_vec size=" << sorted_vec.size() << "\n"; 
    cout<<"total_count="<<total_count<<"\n";

    ofstream bblist_file(bblist_filename);
    ofstream weights_file(weights_filename);

    double wtotal = 0.0;
    for (const auto &pair : sorted_vec)
    {
        int bb = pair.first;
        int count = pair.second;

        double ratio = static_cast<double>(count) / total_count;
        if (wtotal + ratio > th + 1e-9)
            break; // Precision handling

        cout << "bb=" << bb << " ratio=" << ratio << " count=" << count << "\n";

        weights_file << "r" << bb << " "
                     << fixed << setprecision(10) << ratio << " "
                     << fixed << setprecision(10) << ratio << " "
                     << count << endl;
        bblist_file << "r" << bb << endl;

        wtotal += ratio;
        if (wtotal >= th)
            break;
    }

    bblist_file.close();
    weights_file.close();

    return 0;
}