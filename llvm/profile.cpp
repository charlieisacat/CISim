#include "llvm/Pass.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicInst.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <map>
#include <string>

using namespace llvm;
using namespace std;

static cl::opt<int> threshold("threshold",
                                cl::desc("dump threshold"), cl::Optional);

namespace
{
    struct BasicBlockExecCounter : public ModulePass
    {
        static char ID;
        BasicBlockExecCounter() : ModulePass(ID) {}

        uint64_t DUMP_THRESHOLD = 100000; // 10 million instructions

        map<BasicBlock *, GlobalVariable *> bbName;

        FunctionCallee profile_interface;
        FunctionCallee update_flag;
        FunctionCallee x86_time;

        bool runOnModule(Module &M) override
        {
            DUMP_THRESHOLD = threshold;

            // Create a counter for the number of executed instructions
            LLVMContext &Ctx = M.getContext();
            IRBuilder<> Builder(M.getContext());

            auto VoidTy = Type::getVoidTy(Ctx);
            auto I64Ty = Type::getInt64Ty(Ctx);
            auto I32Ty = Type::getInt32Ty(Ctx);
            
            profile_interface = M.getOrInsertFunction("profile_interface", VoidTy, I32Ty, I64Ty, I64Ty, I64Ty);
            update_flag = M.getOrInsertFunction("update_flag", VoidTy, I32Ty);

            x86_time = M.getOrInsertFunction("x86_time", I64Ty);

            for (Function &F : M)
            {
                for (BasicBlock &BB : F)
                {
                    string name = BB.getName().str();
                    name.erase(remove(name.begin(), name.end(), 'r'), name.end());
                    int id = stoi(name);

                    bbName[&BB] = new GlobalVariable(
                        M,
                        I32Ty,
                        true,
                        GlobalValue::ExternalLinkage,
                        ConstantInt::get(I32Ty, id));
                }
            }

            for (Function &F : M)
            {
                for (BasicBlock &BB : F)
                {
                    instrumentBasicBlock(BB, M, Ctx);
                }
            }

            FunctionType *collectTy = FunctionType::get(Builder.getVoidTy(), false);
            Function *start = Function::Create(collectTy, Function::ExternalLinkage,
                                               "start_profiler", &M);
            BasicBlock *startBB = BasicBlock::Create(M.getContext(), "startBB",
                                                     start);
            Builder.SetInsertPoint(startBB);
            appendToGlobalCtors(M, start, 0);

            FunctionCallee init_func = M.getOrInsertFunction("profiler_init", VoidTy);
            Builder.CreateCall(init_func);
            Builder.CreateRetVoid();

            Function *finish = Function::Create(collectTy, Function::ExternalLinkage,
                                                "finish_profiler", &M);
            BasicBlock *finishBB = BasicBlock::Create(M.getContext(), "finishBB",
                                                      finish);
            Builder.SetInsertPoint(finishBB);
            appendToGlobalDtors(M, finish, 0);

            FunctionCallee finish_fnc = M.getOrInsertFunction("profiler_finish", VoidTy);
            Builder.CreateCall(finish_fnc);

            Builder.CreateRetVoid();

            return true;
        }
        
        void instrumentBasicBlock(BasicBlock &BB, Module &M, LLVMContext &Ctx)
        {
            uint64_t InstCount = BB.size();
            IRBuilder<> Builder(&BB);
            Builder.SetInsertPoint(&BB, BB.getFirstInsertionPt());
            Value *blockName = Builder.CreateLoad(Builder.getInt32Ty(), bbName[&BB]);
            Builder.CreateCall(update_flag, {blockName});

            Value *start = Builder.CreateCall(x86_time);
            
            Instruction *Terminator = BB.getTerminator();
            Builder.SetInsertPoint(Terminator);

            Value *end = Builder.CreateCall(x86_time);
            Value *cycles = Builder.CreateSub(end, start);

            auto inc = ConstantInt::get(Type::getInt64Ty(Ctx), InstCount);
            auto threshold = ConstantInt::get(Type::getInt64Ty(Ctx), DUMP_THRESHOLD);
            Builder.CreateCall(profile_interface, {blockName, inc, threshold, cycles});
        }
    };
}

char BasicBlockExecCounter::ID = 0;
static RegisterPass<BasicBlockExecCounter> X("profile", "Basic Block Execution Counter Pass", false, false);
