#include "task/dyn_insn.h"

bool DynamicOperation::isConditional()
{

    if (isBr())
    {
        llvm::Instruction *llvmInsn = static_insn->getLLVMInsn();
        llvm::BranchInst *br = llvm::dyn_cast<llvm::BranchInst>(llvmInsn);

        return br->isConditional();
    }

    return false;
}

void DynamicOperation::updatePortMapping(std::map<uint64_t, std::shared_ptr<InstConfig>> configs, double perc)
{
    allowPorts.clear();
    portStallCycles.clear();

    assert(configs.count(opcode));
    auto config = configs[opcode];

    portMapping = config->ports;

    for (auto mapping : portMapping)
    {
        for (auto portID : mapping.second)
        {
            allowPorts.push_back(portID);
        }
        portStallCycles.push_back(mapping.first);
    }
    fus = config->fus;

    if (isEncapsulateFunc)
    {
        // compute custom instrcution runtime cycle
        int cycle = 0;
        int maxOpLat = 0;
        for (auto op : cpOps)
        {
            int opLat = configs[op]->runtime_cycles;
            // int opLat = 1;

            opLat = (int)ceil(opLat * perc);
            cycle += opLat;
            maxOpLat = std::max(maxOpLat, opLat);
        }
        setRuntimeCycle(cycle);
        // setStageCycle(maxOpLat);
        setStageCycle(1);
    }
    else if (isPackingFunc || isExtractFunc)
    {
        setRuntimeCycle(config->runtime_cycles);
    }
}

bool CallDynamicOperation::shouldRead()
{
    llvm::Instruction *llvmInsn = static_insn->getLLVMInsn();
    llvm::CallInst *call = llvm::dyn_cast<llvm::CallInst>(llvmInsn);

    llvm::Value *calledValue = call->getCalledOperand();
    if (llvm::Function *funcPtr = llvm::dyn_cast<llvm::Function>(calledValue))
    {
        if (funcPtr->isDeclaration())
        {
            // cout << "func = " << op->getName().str() << "can not read\n";
            return false;
        }
    }

    if (llvm::BitCastOperator *bitcastOp = llvm::dyn_cast<llvm::BitCastOperator>(calledValue))
    {
        llvm::Value *op = bitcastOp->getOperand(0);

        if (llvm::Function *funcPtr = llvm::dyn_cast<llvm::Function>(op))
        {
            if (funcPtr->isDeclaration())
            {
                // cout << "func = " << op->getName().str() << "can not read\n";
                return false;
            }
        }
    }

    return true;
}

llvm::Function *CallDynamicOperation::getCalledFunc()
{

    llvm::Instruction *llvmInsn = static_insn->getLLVMInsn();
    llvm::CallInst *call = llvm::dyn_cast<llvm::CallInst>(llvmInsn);

    auto func = call->getCalledFunction();
    return func;
}

llvm::Function *InvokeDynamicOperation::getCalledFunc()
{
    llvm::Instruction *llvmInsn = static_insn->getLLVMInsn();
    llvm::InvokeInst *invoke = llvm::dyn_cast<llvm::InvokeInst>(llvmInsn);

    auto func = invoke->getCalledFunction();
    return func;
}

llvm::Value *SwitchDynamicOperation::getDestValue(int cond)
{
    auto mapit = caseBBMap.find(cond);
    if (mapit != caseBBMap.end())
    {
        return mapit->second;
    }
    return defaultCaseDest;
}

void DynamicOperation::occupyFU(std::vector<int> avalIDs, std::vector<std::shared_ptr<Port>> avalPorts)
{
    usingPorts = avalPorts;
    usingFuIDs = avalIDs;
}

void DynamicOperation::freeFU()
{
    usingPorts.clear();
    usingFuIDs.clear();
}

void PhiDynamicOperation::setPreviousBB(std::shared_ptr<SIM::BasicBlock> previousBB)
{
    assert(previousBB != nullptr);
    llvm::Value *incomingValue = nullptr;

    // auto bb = previousBB->getLLVMBasicBlock();
    // llvm::errs() << "prevBB=" << *bb << "\n";

    llvm::PHINode *phi = llvm::dyn_cast<llvm::PHINode>(getStaticInsn()->getLLVMInsn());
    // Iterate over the incoming values
    for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i)
    {
        llvm::Value *value = phi->getIncomingValue(i);
        llvm::BasicBlock *block = phi->getIncomingBlock(i);

        // llvm::errs() << "inBB=" << *block << "\n";
        if (block == previousBB->getLLVMBasicBlock())
        {
            incomingValue = value;
            break;
        }
    }

    // get incomingValueID
    auto staticDeps = getStaticInsn()->getStaticDependency();
    for (auto value : staticDeps)
    {
        auto insn = std::dynamic_pointer_cast<SIM::Instruction>(value);
        if (insn->getLLVMInsn() == incomingValue)
        {
            incomingValueID = insn->getUID();
            validIncoming = true;
            break;
        }
    }
    // llvm::errs() << "incomingValue=" << *incomingValue << "\n";
}

void DynamicOperation::updateStatus(DynOpStatus _status)
{
    status = _status;
}

void DynamicOperation::updateStatus()
{
    if (status == DynOpStatus::INIT)
    {
        status = DynOpStatus::ALLOCATED;
    }
    else if (status == DynOpStatus::ALLOCATED)
    {
        status = DynOpStatus::READY;
    }
    else if (status == DynOpStatus::READY)
    {
        status = DynOpStatus::ISSUED;
    }
    else if (status == DynOpStatus::ISSUED)
    {
        status = DynOpStatus::EXECUTING;
    }
    else if (status == DynOpStatus::EXECUTING)
    {
        status = DynOpStatus::FINISHED;
    }
    else if (status == DynOpStatus::FINISHED)
    {
        status = DynOpStatus::COMMITED;
    }
}

void DynamicOperation::execute()
{
    if (!isLoad())
    {
        // load status is changed in lsu
        if (status == DynOpStatus::ISSUED)
            updateStatus();
    }
    currentCycle++;
}

void DynamicOperation::removeDep(uint64_t uid)
{
    auto end = deps.end();
    auto it = deps.begin();
    for (; it != end; it++)
    {
        if ((*it)->getUID() == uid)
            break;
    }

    if (it != end)
        deps.erase(it);
}

void DynamicOperation::signalUser()
{
    for (std::shared_ptr<DynamicOperation> user : users)
    {
        user->removeDep(getUID());
    }
}