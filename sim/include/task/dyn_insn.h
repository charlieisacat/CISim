#ifndef INCLUDE_DYN_INSN_H
#define INCLUDE_DYN_INSN_H

#include "llvm.h"
#include "instruction.h"
#include "params.h"
#include "basic_block.h"
#include "port.h"

#include <stdio.h>
#include <iostream>
#include "utils.h"

using namespace std;

class Port;

enum DynOpStatus
{
    INIT,
    ALLOCATED,
    READY,
    ISSUED,
    EXECUTING,
    FINISHED, // to commit
    COMMITED, // indicate that it can be fired to memory
    SLEEP     // status to indicate load is waiting for forwarding
};

// dynamic (custom) instructions created in runtime
class DynamicOperation
{
public:
    DynamicOperation() {}
    DynamicOperation(std::shared_ptr<SIM::Instruction> _static_insn, std::shared_ptr<InstConfig> config) : static_insn(_static_insn),
                                                                                                           totalCycle(config->runtime_cycles),
                                                                                                           portMapping(config->ports), fus(config->fus)
    {
        for (auto mapping : portMapping)
        {
            for (auto portID : mapping.second)
            {
                allowPorts.push_back(portID);
            }
            portStallCycles.push_back(mapping.first);
        }

        opcode = static_insn->getOpCode();

        llvm::Type *returnType = static_insn->getLLVMInsn()->getType();

        if (returnType->isFloatingPointTy())
        {
            isInt = false;
        }
    }

    void updatePortMapping(std::map<uint64_t, std::shared_ptr<InstConfig>> configs, double perc);

    bool isInt = true;

    virtual bool isPhi() { return false; }
    virtual bool isCall() { return false; }
    virtual bool isLoad() { return false; }
    virtual bool isStore() { return false; }
    virtual bool isArith() { return false; }
    virtual bool isRet() { return false; }
    virtual bool isBr() { return false; }
    virtual bool isSwitch() { return false; }
    virtual bool isInvoke() { return false; }

    std::string getOpName() { return static_insn->getOpName(); }
    int getOpCode() { return opcode; }

    std::shared_ptr<SIM::Instruction> getStaticInsn() { return static_insn; }
    uint64_t getUID() { return static_insn->getUID(); }

    void setCurrentCycle(uint64_t _cycle) { currentCycle = _cycle; }
    virtual bool finish() { return currentCycle >= totalCycle; }
    void execute();

    // load is finished
    void setFinish(bool f) { finished = f; }

    std::vector<int> getPorts() { return portMapping[0].second; }
    std::vector<int> getAllowPorts() { return allowPorts; }

    virtual std::vector<uint64_t> getStaticDepUIDs() { return static_insn->getStaticDepUIDs(); }
    std::vector<uint64_t> getDynDeps() { return dynDeps; }
    std::vector<uint64_t> getDynUsers() { return dynUsers; }
    void addDynDeps(uint64_t dep) { dynDeps.push_back(dep); }
    void clearDynDeps() { dynDeps.clear(); }

    uint64_t getDynID() { return dynID; }
    virtual uint64_t getAddr() { return addr; }
    void updateDynID(uint64_t _dynID) { dynID = _dynID; }

    bool isSquashed = false;

    // used for function without body
    uint64_t cycle = 0;

    void reset()
    {
        setCurrentCycle(0);
        if (isLoad() || isStore())
            setFinish(false);
        isSquashed = false;
        status = DynOpStatus::INIT;
        E = 0;
        e = 0;
        D = 0;
        R = 0;
        clearDynDeps();
        freeFU();
        mispredict = false;
    }

    // use this to create different dyn instances but having same dynID
    // because we want to track if insn is squashed
    std::shared_ptr<DynamicOperation> clone() const { return std::static_pointer_cast<DynamicOperation>(createClone()); }
    virtual std::shared_ptr<DynamicOperation> createClone() const { return std::shared_ptr<DynamicOperation>(new DynamicOperation(*this)); }

    // State Machine: state tranfer
    virtual void updateStatus(DynOpStatus _status);
    virtual void updateStatus();

    DynOpStatus status = DynOpStatus::INIT;

    // llvm-mca style timeline
    uint64_t D = 0; // dispatch
    uint64_t e = 0; // execute
    uint64_t E = 0; // writeback
    uint64_t R = 0; // retire

    std::vector<std::vector<int>> fus;

    // occupied port and fu

    std::vector<int> usingFuIDs;
    std::vector<std::shared_ptr<Port>> usingPorts;

    void occupyFU(std::vector<int> avalIDs, std::vector<std::shared_ptr<Port>> avalPorts);
    void freeFU();

    std::vector<std::pair<int, std::vector<int>>> portMapping;
    std::vector<int> portStallCycles;

    // port id
    std::vector<int> allowPorts;

    std::string calleeName = "";
    std::string parentFuncName = "";

    bool mispredict = false;

    bool isExtractFunc = false;
    bool isPackingFunc = false;
    bool isEncapsulateFunc = false;
    std::vector<int> cpOps;
    std::vector<int> fuOps;

    void setRuntimeCycle(uint64_t cycle) { totalCycle = cycle; }

    uint32_t opcode = -1;
    int dyserUnitID = -1;

    std::vector<uint64_t> readRFUids;

    bool freeFUEarly() { return isEncapsulateFunc && (currentCycle >= stageCycle); }
    void setStageCycle(uint64_t _cycle) { stageCycle = _cycle; }

    uint64_t getTotalCycle() { return totalCycle; }

    // this is only used for accel
    // dependency count == 0
    bool ready() { return deps.size() == 0; }

    // for accel
    void addUser(std::shared_ptr<DynamicOperation> user) { users.push_back(user); }
    void addDep(std::shared_ptr<DynamicOperation> dep) { deps.push_back(dep); }
    std::vector<std::shared_ptr<DynamicOperation>> getDeps() { return deps; }

    void signalUser();
    void removeDep(uint64_t uid);

    // for dyser
    void addUserDynID(uint64_t user) { dynUsers.push_back(user); }

    int offset = -1;
    int stride = -1;

    std::shared_ptr<SIM::BasicBlock> targetBB = nullptr;
    uint64_t globalDynID = 0;
    bool offloaded = false;

    std::shared_ptr<DynamicOperation> prevBBTerminator = nullptr;
    bool isConditional();

    bool waitingForBrDecision = true;

protected:
    std::shared_ptr<SIM::Instruction> static_insn;

    uint64_t totalCycle = 1;
    uint64_t currentCycle = 0;
    uint64_t stageCycle = 1;

    std::vector<uint64_t> dynDeps;
    std::vector<uint64_t> dynUsers;

    std::vector<std::shared_ptr<DynamicOperation>> deps;
    std::vector<std::shared_ptr<DynamicOperation>> users;

    uint64_t dynID;

    uint64_t addr = 0;

    bool finished = false;
};

enum FuncType
{
    EXTERNAL,
    INTERNAL,
    INVALID
};

class SwitchDynamicOperation : public DynamicOperation
{
public:
    SwitchDynamicOperation(std::shared_ptr<SIM::Instruction> _static_insn, std::shared_ptr<InstConfig> config) : DynamicOperation(_static_insn, config)
    {

        llvm::Instruction *llvmInsn = static_insn->getLLVMInsn();
        llvm::SwitchInst *switchInsn = llvm::dyn_cast<llvm::SwitchInst>(llvmInsn);

        for (auto &Case : switchInsn->cases())
        {
            uint64_t caseValue = std::move(Case.getCaseValue()->getSExtValue());
            llvm::BasicBlock *destBB = Case.getCaseSuccessor();
            // std::cout << "caseValue=" << caseValue << "bbname=" << destBB->getName().str() << "\n";
            caseBBMap.insert(std::make_pair<int, llvm::Value *>(caseValue, destBB));
        }

        defaultCaseDest = switchInsn->getDefaultDest();
        // std::cout << "defualt case=" << defaultCaseDest->getName().str() << "\n";
    }
    virtual bool isSwitch() override { return true; }
    llvm::Value *getDestValue(int cond);
    int getCondition() { return cond; }
    void setCondition(int _cond) { cond = _cond; }

    virtual std::shared_ptr<DynamicOperation> createClone() const override { return std::shared_ptr<SwitchDynamicOperation>(new SwitchDynamicOperation(*this)); }

private:
    std::map<int, llvm::Value *> caseBBMap;
    llvm::Value *defaultCaseDest = nullptr;
    int cond = 0;
};

class CallDynamicOperation : public DynamicOperation
{
public:
    CallDynamicOperation(std::shared_ptr<SIM::Instruction> _static_insn, std::shared_ptr<InstConfig> config) : DynamicOperation(_static_insn, config)
    {
        llvm::CallInst *callInst = llvm::dyn_cast<llvm::CallInst>(static_insn->getLLVMInsn());
        auto func = getCalledFunc();
        // calleeName = func->getName().str();
        if (func)
        {
            // Retrieve custom metadata by key
            if (llvm::MDNode *CustomMD = func->getMetadata(func->getName().str()))
            {
                hasBody = false;
                isEncapsulateFunc = true;
                int i = 0;
                for (const auto &op : CustomMD->operands())
                {
                    auto *meta = llvm::dyn_cast<llvm::MDString>(op.get());
                    if (i == 0)
                    {
                        opcode = std::stoi(meta->getString().str());
                    }
                    else if (i == 1)
                    {
                        cpOps = splitAndConvertToInt(meta->getString().str());
                    }
                    else if (i == 2)
                    {
                        fuOps = splitAndConvertToInt(meta->getString().str());
                    }

                    i++;
                }
            }
            else if (llvm::MDNode *CustomMD = func->getMetadata("packing_func"))
            {
                opcode = 1023;
                hasBody = false;
                isPackingFunc = true;

                auto *MDStr = llvm::dyn_cast<llvm::MDString>(CustomMD->getOperand(0));
                dyserUnitID = std::stoi(MDStr->getString().str());

                // for (const auto &op : CustomMD->operands())
                // {
                //     std::cout<<"     op\n";

                //     auto *meta = llvm::dyn_cast<llvm::MDString>(op.get());
                //     dyserUnitID = std::stoi(meta->getString().str());
                //     break;
                // }

                // this works only for dyser
                // even novia will have first constant arg, we will ignore it
                // Exclude the function itself
                if (callInst->getNumOperands() - 1 > 0)
                {
                    llvm::Value *arg = callInst->getArgOperand(0);
                    if (llvm::ConstantInt *constInt = llvm::dyn_cast<llvm::ConstantInt>(arg))
                    {
                        offset = constInt->getValue().getSExtValue();
                        stride = callInst->getNumOperands() - 2;
                    }
                }
            }
            else if (llvm::MDNode *CustomMD = func->getMetadata("extract_func"))
            {
                opcode = 1024;
                hasBody = false;
                isExtractFunc = true;

                auto *MDStr = llvm::dyn_cast<llvm::MDString>(CustomMD->getOperand(0));
                dyserUnitID = std::stoi(MDStr->getString().str());

                if (callInst->getNumOperands() - 1 > 0)
                {
                    llvm::Value *arg = callInst->getArgOperand(0);
                    if (llvm::ConstantInt *constInt = llvm::dyn_cast<llvm::ConstantInt>(arg))
                    {
                        offset = constInt->getValue().getSExtValue();
                        stride = 1;
                    }
                }
            }
            else
            {
                // no metadata
                if (func->isDeclaration() || func->isIntrinsic())
                {
                    hasBody = false;
                }
                else
                {
                    hasBody = true;
                }
            }
        }
        else
        {
            hasBody = false;
        }
    }
    virtual bool isCall() override { return true; }

    void setFuncType(FuncType _funcType) { funcType = _funcType; }
    llvm::Function *getCalledFunc();
    bool shouldRead();

    virtual std::shared_ptr<DynamicOperation> createClone() const override { return std::shared_ptr<CallDynamicOperation>(new CallDynamicOperation(*this)); }

    virtual bool finish() override
    {
        if (isExtractFunc)
        {
            return finished && (currentCycle >= totalCycle);
        }

        return currentCycle >= totalCycle;
    }

    bool hasBody = false;

private:
    uint64_t latency = 0;
    FuncType funcType = FuncType::INVALID;
};

class LoadDynamicOperation : public DynamicOperation
{
public:
    LoadDynamicOperation(std::shared_ptr<SIM::Instruction> _static_insn, std::shared_ptr<InstConfig> config) : DynamicOperation(_static_insn, config) {}
    virtual bool isLoad() override { return true; }
    virtual uint64_t getAddr() override { return addr; }
    void setAddr(uint64_t _addr) { addr = _addr; }

    virtual bool finish() override { return finished && (currentCycle >= totalCycle); }

    virtual std::shared_ptr<DynamicOperation> createClone() const override { return std::shared_ptr<LoadDynamicOperation>(new LoadDynamicOperation(*this)); }

private:
    uint64_t addr = 0;
};

class StoreDynamicOperation : public DynamicOperation
{
public:
    StoreDynamicOperation(std::shared_ptr<SIM::Instruction> _static_insn, std::shared_ptr<InstConfig> config) : DynamicOperation(_static_insn, config) {}
    virtual bool isStore() override { return true; }
    virtual uint64_t getAddr() override { return addr; }
    void setAddr(uint64_t _addr) { addr = _addr; }

    virtual bool finish() override { return currentCycle >= totalCycle; }
    virtual std::shared_ptr<DynamicOperation> createClone() const override { return std::shared_ptr<StoreDynamicOperation>(new StoreDynamicOperation(*this)); }

private:
    uint64_t addr = 0;
};

class ArithDynamicOperation : public DynamicOperation
{
public:
    ArithDynamicOperation(std::shared_ptr<SIM::Instruction> _static_insn, std::shared_ptr<InstConfig> config) : DynamicOperation(_static_insn, config) {}
    virtual bool isArith() override { return true; }

    virtual std::shared_ptr<DynamicOperation> createClone() const override { return std::shared_ptr<ArithDynamicOperation>(new ArithDynamicOperation(*this)); }
};

class RetDynamicOperation : public DynamicOperation
{
public:
    RetDynamicOperation(std::shared_ptr<SIM::Instruction> _static_insn, std::shared_ptr<InstConfig> config) : DynamicOperation(_static_insn, config)
    {

        llvm::Instruction *llvmInsn = static_insn->getLLVMInsn();
        llvm::ReturnInst *ret = llvm::dyn_cast<llvm::ReturnInst>(llvmInsn);

        llvm::Function *parentFunc = ret->getFunction();
        if (parentFunc)
        {
            parentFuncName = parentFunc->getName();
        }
    }
    virtual bool isRet() override { return true; }

    virtual std::shared_ptr<DynamicOperation> createClone() const override { return std::shared_ptr<RetDynamicOperation>(new RetDynamicOperation(*this)); }
};

class BrDynamicOperation : public DynamicOperation
{
public:
    BrDynamicOperation(std::shared_ptr<SIM::Instruction> _static_insn, std::shared_ptr<InstConfig> config) : DynamicOperation(_static_insn, config)
    {
        llvm::Instruction *llvmInsn = static_insn->getLLVMInsn();
        llvm::BranchInst *br = llvm::dyn_cast<llvm::BranchInst>(llvmInsn);

        trueDestValue = br->getSuccessor(0);

        if (br->isConditional())
            falseDestValue = br->getSuccessor(1);
    }
    virtual bool isBr() override { return true; }
    bool getCondition() { return cond; }
    llvm::Value *getDestValue(bool cond) { return cond == 0 ? falseDestValue : trueDestValue; }
    void setCondition(bool _cond) { cond = _cond; }

    virtual std::shared_ptr<DynamicOperation> createClone() const override { return std::shared_ptr<BrDynamicOperation>(new BrDynamicOperation(*this)); }

private:
    bool cond = false;
    llvm::Value *trueDestValue = nullptr;
    llvm::Value *falseDestValue = nullptr;
};

class InvokeDynamicOperation : public DynamicOperation
{
public:
    InvokeDynamicOperation(std::shared_ptr<SIM::Instruction> _static_insn, std::shared_ptr<InstConfig> config) : DynamicOperation(_static_insn, config)
    {
        llvm::Instruction *llvmInsn = static_insn->getLLVMInsn();
        llvm::InvokeInst *invoke = llvm::dyn_cast<llvm::InvokeInst>(llvmInsn);
        // llvm::errs()<<*invoke<<"\n";

        auto func = getCalledFunc();
        if (func)
        {
            if (func->isDeclaration() || func->isIntrinsic())
            {
                // llvm::errs()<<"--------- false\n";
                hasBody = false;
            }
            else
            {
                // llvm::errs()<<"-------- true\n";
                hasBody = true;
            }
        }
        else
        {
            hasBody = false;
        }

        normalDest = invoke->getNormalDest();
        // llvm::errs()<<*normalDest<<"\n";
    }

    virtual bool isInvoke() override { return true; }
    virtual std::shared_ptr<DynamicOperation> createClone() const override { return std::shared_ptr<InvokeDynamicOperation>(new InvokeDynamicOperation(*this)); }
    llvm::Function *getCalledFunc();
    llvm::Value *getDestValue() { return normalDest; }

    bool hasBody = false;

private:
    llvm::BasicBlock *normalDest = nullptr;
};

class PhiDynamicOperation : public DynamicOperation
{
public:
    PhiDynamicOperation(std::shared_ptr<SIM::Instruction> _static_insn, std::shared_ptr<InstConfig> config) : DynamicOperation(_static_insn, config) {}
    virtual bool isPhi() override { return true; }
    virtual std::vector<uint64_t> getStaticDepUIDs() override
    {
        if (validIncoming)
            return {incomingValueID};
        return {};
    }
    void setPreviousBB(std::shared_ptr<SIM::BasicBlock> previousBB);

    virtual std::shared_ptr<DynamicOperation> createClone() const override { return std::shared_ptr<PhiDynamicOperation>(new PhiDynamicOperation(*this)); }

private:
    // std::shared_ptr<SIM::BasicBlock> previousBB = nullptr;

    uint64_t incomingValueID = 0;
    bool validIncoming = false;
};
#endif
