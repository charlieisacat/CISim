#include "llvm/Pass.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Support/CommandLine.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>

#include "llvm/Support/raw_ostream.h"

#include <string>
#include <map>
#include <fstream>
#include <set>

using namespace llvm;
using namespace std;

struct renameBBs : public ModulePass
{
    static char ID;

    map<string, unsigned> Names;

    renameBBs() : ModulePass(ID) {}

    bool runOnModule(Module &M) override
    {
        int i = 0;
        for (Function &F : M)
        {
            for (BasicBlock &BB : F)
            {
                BB.setName("r" + to_string(i));
                i++;
            }
        }
        return true;
    }
};

struct markInline : public ModulePass
{
    static char ID;

    markInline() : ModulePass(ID) {}

    bool runOnModule(Module &M) override
    {
        for (Function &F : M)
        {
            if (!F.isDeclaration())
            {
                F.removeFnAttr(Attribute::OptimizeNone);
                if (F.getName().str() != "main")
                {
                    F.removeFnAttr(Attribute::NoInline);
                    F.addFnAttr(Attribute::AlwaysInline);
                }
            }
        }
        return true;
    }
};

char renameBBs::ID = 0;
static RegisterPass<renameBBs> a("renameBBs", "Renames BBs with unique names",
                                 false, false);
static RegisterPass<markInline> b("markInline", "Mark functions as always inline",
                                  false, false);

static RegisterStandardPasses A(
    PassManagerBuilder::EP_EarlyAsPossible,
    [](const PassManagerBuilder &Builder,
       legacy::PassManagerBase &PM)
    { PM.add(new renameBBs()); });

char markInline::ID = 1;
static RegisterStandardPasses B(
    PassManagerBuilder::EP_EarlyAsPossible,
    [](const PassManagerBuilder &Builder,
       legacy::PassManagerBase &PM)
    { PM.add(new markInline()); });