#include <string>
#include <filesystem>
#include <map>

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

#include "yaml-cpp/yaml.h"

using namespace llvm;
using namespace std;

void move_insn_before(Instruction *insn, Instruction *target)
{
    insn->removeFromParent();
    insn->insertBefore(target);
}

void schedule(
    map<Instruction *, uint64_t> insn_uid_map,
    Module &M)
{

    std::map<Instruction *, Instruction *> targetMap;
    for (auto &F : M)
    {
        string func_name = F.getName().str();
        if (func_name.find("encapsulated_functio") != std::string::npos)
        {
            continue;
        }

        if (func_name.find("packing_function") != std::string::npos)
        {
            continue;
        }

        for (auto &BB : F)
        {
            for (auto &I : BB)
            {
                uint64_t min_use_insn_uid = 1e8;
                Instruction *target = nullptr;

                string name = I.getName().str();
                if (name.find("extracted") == std::string::npos)
                    continue;

                for (auto &use : I.uses())
                {
                    if (Instruction *user = dyn_cast<Instruction>(use.getUser()))
                    {
                        if (user->getParent()->getName().str() != I.getParent()->getName().str())
                            continue;

                        if (isa<PHINode>(user))
                            continue;

                        uint64_t uid = insn_uid_map[user];
                        if (uid < min_use_insn_uid)
                        {
                            min_use_insn_uid = uid;
                            target = user;
                        }
                    }
                }

                if (target)
                {
                    targetMap.insert(std::make_pair(&I, target));
                }
            }
        }
    }

    for (auto item : targetMap)
    {
        move_insn_before(item.first, item.second);
        // Verify the module
        if (verifyModule(M, &llvm::errs()))
        {
            errs() << "from=" << *item.first << " to=" << *item.second << "\n";
            errs() << "Error: Move failed!\n";
            return;
        }
    }
}

int main(int argc, char **argv)
{
    std::string ir_path = std::string(argv[1]);
    std::string output = std::string(argv[2]);

    LLVMContext context;
    SMDiagnostic err;

    // Load the LLVM module from a file
    std::unique_ptr<Module> M = parseIRFile(ir_path, err, context);

    uint64_t uid = 0;
    map<Instruction *, uint64_t> insn_uid_map;
    for (auto &F : *M)
    {
        for (auto &BB : F)
        {
            for (auto &I : BB)
            {
                insn_uid_map.insert(make_pair(&I, uid));
                uid++;
            }
        }
    }

    schedule(insn_uid_map, *M);

    // Write the modified module to a file
    std::error_code EC;
    raw_fd_ostream OS(output+"_sch.ll", EC, sys::fs::OF_None);
    if (EC)
    {
        errs() << "Could not open file: " << EC.message();
        return 1;
    }

    M->print(OS, nullptr);
    OS.flush();

    return 0;
}