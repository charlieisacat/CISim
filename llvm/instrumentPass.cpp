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

#include <string>
#include <vector>
#include <map>

using namespace llvm;
using namespace std;

static cl::opt<uint64_t> tgt_threshold("threshold",
                                       cl::desc("dump threshold"), cl::Optional);
static cl::opt<uint64_t> tgt_interval("interval",
                                      cl::desc("interval"), cl::Optional);
static cl::opt<int> simpoint("simpoint",
                             cl::desc("simpoint"), cl::Optional);

namespace
{
    struct instrumentPass : public ModulePass
    {
        static char ID;

        instrumentPass() : ModulePass(ID) {}
        GlobalVariable *TotalInstCount = nullptr;
        GlobalVariable *T = nullptr;
        FunctionCallee update_insncount_interval;
        FunctionCallee update_dump_status;
        GlobalVariable *global_flag_value = nullptr;

        map<BasicBlock *, GlobalVariable *> bbName;
        map<Function *, GlobalVariable *> func_name_map;

        void instrumentBasicBlock(BasicBlock &BB, Module &M)
        {
            LLVMContext &Ctx = BB.getContext();
            IRBuilder<> Builder(&BB);
            Instruction *Terminator = BB.getTerminator();
            Builder.SetInsertPoint(Terminator);

            uint64_t InstCount = BB.size();
            auto inc = ConstantInt::get(Type::getInt64Ty(Ctx), InstCount);
            auto threshold = ConstantInt::get(Type::getInt64Ty(Ctx), tgt_threshold);
            auto interval = ConstantInt::get(Type::getInt64Ty(Ctx), tgt_interval);

            Value *inst_count_ptr = Builder.CreateBitCast(TotalInstCount, Type::getInt64PtrTy(Ctx));
            Value *interval_ptr = Builder.CreateBitCast(T, Type::getInt64PtrTy(Ctx));

            Builder.CreateCall(update_insncount_interval, {inst_count_ptr, interval_ptr, threshold, inc, bbName[&BB]});

            Builder.SetInsertPoint(&BB, BB.getFirstInsertionPt());
            Value *flag_ptr = Builder.CreateBitCast(global_flag_value, Type::getInt8PtrTy(Ctx));
            Function *func = BB.getParent();
            Value *func_name = Builder.CreateBitCast(func_name_map[func], Type::getInt8PtrTy(Ctx));
            Builder.CreateCall(update_dump_status, {interval_ptr, interval, flag_ptr, bbName[&BB], func_name});
        }

        bool runOnModule(Module &M) override
        {
            LLVMContext &Ctx = M.getContext();
            auto I64Ty = Type::getInt64Ty(Ctx);
            auto I32Ty = Type::getInt32Ty(Ctx);
            auto VoidTy = Type::getVoidTy(Ctx);
            auto I8Ty = Type::getInt8Ty(Ctx);

            IRBuilder<> Builder(Ctx);

            FunctionCallee dump_data_func = M.getOrInsertFunction("log_helper_dump_data", VoidTy, I64Ty, I8Ty);

            errs() << "thre=" << tgt_threshold << " inter=" << tgt_interval << " simpoint=" << simpoint << "\n";

            global_flag_value = new GlobalVariable(
                M,
                Type::getInt8Ty(Ctx),                                     // Type: uint8_t (i8)
                false,                                                    // isConstant
                GlobalValue::ExternalLinkage,                             // linkage type
                ConstantInt::get(Type::getInt8Ty(Ctx), simpoint ? 0 : 1), // initializer
                "global_flag_value"                                       // name
            );

            TotalInstCount = new GlobalVariable(
                M, Type::getInt64Ty(Ctx), false, GlobalValue::CommonLinkage,
                ConstantInt::get(Type::getInt64Ty(Ctx), 0), "TotalInstCount");

            T = new GlobalVariable(
                M, Type::getInt64Ty(Ctx), false, GlobalValue::CommonLinkage,
                ConstantInt::get(Type::getInt64Ty(Ctx), 0), "T");

            update_insncount_interval = M.getOrInsertFunction("update_insncount_interval", VoidTy, Type::getInt64PtrTy(Ctx), Type::getInt64PtrTy(Ctx), I64Ty, I64Ty,  Type::getInt32PtrTy(Ctx));
            update_dump_status = M.getOrInsertFunction("update_dump_status", VoidTy, Type::getInt64PtrTy(Ctx), I64Ty, Type::getInt8PtrTy(Ctx), Type::getInt32PtrTy(Ctx), Type::getInt8PtrTy(Ctx));

            if (simpoint)
            {
                for (Function &F : M)
                {
                    if (!F.isDeclaration() && !F.isIntrinsic())
                    {
                        string fname = F.getName().str();

                        func_name_map[&F] = new GlobalVariable(
                            M,
                            ArrayType::get(Type::getInt8Ty(Ctx), fname.size() + 1), // +1 for null terminator
                            true,                                                   // isConstant
                            GlobalValue::ExternalLinkage,
                            nullptr);

                        // Create an initializer for the global variable
                        std::vector<Constant *> char_array;
                        for (char c : fname)
                        {
                            char_array.push_back(ConstantInt::get(Type::getInt8Ty(Ctx), c));
                        }
                        char_array.push_back(ConstantInt::get(Type::getInt8Ty(Ctx), 0)); // null terminator

                        // Create a global variable initializer
                        func_name_map[&F]->setInitializer(ConstantArray::get(ArrayType::get(Type::getInt8Ty(Ctx), fname.size() + 1), char_array));
                    }

                    for (BasicBlock &BB : F)
                    {
                        string name = BB.getName().str();
                        name.erase(remove(name.begin(), name.end(), 'r'), name.end());
                        int id = stoi(name);

                        assert(!bbName.count(&BB));

                        bbName[&BB] = new GlobalVariable(
                            M,
                            I32Ty,
                            true,
                            GlobalValue::ExternalLinkage,
                            ConstantInt::get(I32Ty, id));
                    }
                }

                for (auto &F : M)
                {
                    for (BasicBlock &BB : F)
                    {
                        instrumentBasicBlock(BB, M);
                    }
                }
            }

            for (auto &F : M)
            {
                auto fname = F.getName().str();
                if (fname == "update_dump_status" ||
                    fname == "update_insncount_interval" ||
                    fname == "log_helper_dump_data" ||
                    fname == "log_helper_flush_buffer" ||
                    fname == "log_helper_init" ||
                    fname == "log_helper_finish")
                    continue;

                for (BasicBlock &BB : F)
                {
                    for (Instruction &I : BB)
                    {
                        if (LoadInst *LI = dyn_cast<LoadInst>(&I))
                        {
                            IRBuilder<> Builder(LI);
                            Value *Address = LI->getPointerOperand();
                            if (Address == global_flag_value || Address == TotalInstCount || Address == T)
                            {
                                continue;
                            }

                            Value *flag_value = Builder.CreateLoad(global_flag_value->getValueType(), global_flag_value);
                            Value *CastAddress = Builder.CreatePtrToInt(Address, I64Ty);
                            Builder.CreateCall(dump_data_func, {CastAddress, flag_value});
                        }
                        else if (StoreInst *SI = dyn_cast<StoreInst>(&I))
                        {
                            IRBuilder<> Builder(SI);
                            Value *Address = SI->getPointerOperand();
                            if (Address == global_flag_value || Address == TotalInstCount || Address == T)
                            {
                                continue;
                            }

                            Value *flag_value = Builder.CreateLoad(global_flag_value->getValueType(), global_flag_value);
                            Value *CastAddress = Builder.CreatePtrToInt(Address, I64Ty);
                            Builder.CreateCall(dump_data_func, {CastAddress, flag_value});
                        }
                        else if (BranchInst *BI = dyn_cast<BranchInst>(&I))
                        {
                            IRBuilder<> Builder(BI);
                            Value *Cond = nullptr;

                            if (BI->isConditional())
                            {
                                Cond = BI->getCondition();
                                Value *flag_value = Builder.CreateLoad(global_flag_value->getValueType(), global_flag_value);
                                Value *CondAsInt = Builder.CreateZExtOrBitCast(Cond, I64Ty);
                                Builder.CreateCall(dump_data_func, {CondAsInt, flag_value});
                            }
                        }
                        else if (SwitchInst *SWI = dyn_cast<SwitchInst>(&I))
                        {
                            IRBuilder<> Builder(SWI);
                            Value *Cond = nullptr;
                            Cond = SWI->getCondition();

                            Value *CondAsInt = Builder.CreateZExtOrBitCast(Cond, I64Ty);
                            Value *flag_value = Builder.CreateLoad(global_flag_value->getValueType(), global_flag_value);
                            Builder.CreateCall(dump_data_func, {CondAsInt, flag_value});
                        }
                    }
                }
            }

            FunctionType *collectTy = FunctionType::get(Builder.getVoidTy(), false);
            Function *start = Function::Create(collectTy, Function::ExternalLinkage,
                                               "tracer_start", &M);
            BasicBlock *startBB = BasicBlock::Create(M.getContext(), "startBB",
                                                     start);
            Builder.SetInsertPoint(startBB);
            appendToGlobalCtors(M, start, 0);

            FunctionCallee init_func = M.getOrInsertFunction("log_helper_init", VoidTy);
            Builder.CreateCall(init_func);
            Builder.CreateRetVoid();

            Function *finish = Function::Create(collectTy, Function::ExternalLinkage,
                                                "tracer_finish", &M);
            BasicBlock *finishBB = BasicBlock::Create(M.getContext(), "finishBB",
                                                      finish);
            Builder.SetInsertPoint(finishBB);
            appendToGlobalDtors(M, finish, 0);

            FunctionCallee finish_fnc = M.getOrInsertFunction("log_helper_finish", VoidTy);
            Builder.CreateCall(finish_fnc);
            Builder.CreateRetVoid();

            return true;
        }
    };
}

char instrumentPass::ID = 0;
static RegisterPass<instrumentPass> a("instrumentPass", "Instrumentation Pass",
                                      false, false);

static RegisterStandardPasses A(
    PassManagerBuilder::EP_EarlyAsPossible,
    [](const PassManagerBuilder &Builder,
       legacy::PassManagerBase &PM)
    { PM.add(new instrumentPass()); });