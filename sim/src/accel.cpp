#include "accel.h"
#include "core.h"

using namespace llvm;

DyserUnit::DyserUnit(std::shared_ptr<SIM::Task> _task,
                     std::shared_ptr<Params> _params, shared_ptr<SIM::BasicBlock> entry) : Hardware(_task, _params)
{
    stat = std::make_shared<Statistic>();
    pam = new PowerAreaModel();

    setEntry(entry);

    fetch();
}

Accelerator::Accelerator(std::shared_ptr<SIM::Task> _task,
                         std::shared_ptr<Params> _params) : Hardware(_task, _params)
{
    stat = std::make_shared<Statistic>();
    pam = new PowerAreaModel();

    // targetBB = fetchEntryBB();
    // lsu = std::make_shared<LSUnit>(nullptr);
}

// same as GEM5-SALAM
void Accelerator::findDynamicDeps(shared_ptr<DynamicOperation> insn)
{
    // cout<<"dep dynid="<<insn->getDynID()<<"\n";
    vector<uint64_t> dep_uids = insn->getStaticDepUIDs();
    auto queue_iter = reservation.rbegin();
    while ((queue_iter != reservation.rend()) && !dep_uids.empty())
    {
        auto queued_inst = *queue_iter;
        // Look at each instruction in runtime queue once
        for (auto dep_it = dep_uids.begin(); dep_it != dep_uids.end();)
        {
            // Check if any of the instruction to be scheduled dependencies match the current instruction from queue
            if (queued_inst->getUID() == *dep_it)
            {
                // If dependency found, create two way link
                insn->addDep(queued_inst);
                // cout << "     =====dynid=" << queued_inst->getDynID() << "\n";
                queued_inst->addUser(insn);

                dep_it = dep_uids.erase(dep_it);
            }
            else
            {
                dep_it++;
            }
        }
        queue_iter++;
    }

    // The other queues do not need to be reverse-searched since only 1 instance of any instruction can exist in them
    // Check the compute queue
    for (auto dep_it = dep_uids.begin(); dep_it != dep_uids.end();)
    {
        auto queue_iter = inflightQueue.find(*dep_it);
        if (queue_iter != inflightQueue.end())
        {
            auto queued_inst = queue_iter->second;

            insn->addDep(queued_inst);
            queued_inst->addUser(insn);
            // cout << "     -----dynid=" << queued_inst->getDynID() << "\n";

            dep_it = dep_uids.erase(dep_it);
        }
        else
        {
            dep_it++;
        }
    }
}

void Accelerator::printStats()
{
    cout << "acc totalFetch=" << totalFetch << "\n";
    cout << "acc totalCommit=" << totalCommit << "\n";
    cout << "acc totalCycle=" << cycles << "\n";
}

#if 1
void Accelerator::processQueue()
{
    lsu->used_port = 0;

    // cout<<"lsu_port="<<lsu->lsu_port<<" used_port="<<lsu->used_port<<"\n";

    // cout << "score size=" << scoreboard.size() << " inflgi size=" << inflightQueue.size() << " resv size=" << reservation.size() << "\n";

    for (auto queue_iter = inflightQueue.begin(); queue_iter != inflightQueue.end();)
    {
        if (commitInsn(queue_iter->second))
        {
            if (!queue_iter->second->isLoad() && !queue_iter->second->isStore())
            {
                // cout<<"commit = "<<queue_iter->second->getDynID()<<" name="<<queue_iter->second->getOpName()<<"\n";
                queue_iter->second->updateStatus(DynOpStatus::COMMITED);
            }

            queue_iter = inflightQueue.erase(queue_iter);
            totalCommit++;
        }
        else
        {
            ++queue_iter;
        }
    }

    for (auto queue_iter = scoreboard.begin(); queue_iter != scoreboard.end();)
    {

        auto inst = *queue_iter;

        if (inst->finish() && inst->status >= DynOpStatus::ISSUED)
        {
            // cout<<"sc remove = "<<inst->getDynID()<<" name="<<inst->getOpName()<<"\n";
            inst->updateStatus(DynOpStatus::COMMITED);
            queue_iter = scoreboard.erase(queue_iter);
        }
        else
        {
            // cout<<"sc not remove = "<<inst->getDynID()<<" name="<<inst->getOpName()<<" size="<<scoreboard.size()<< "status ="<<inst->status<<" fini="<<inst->finish()<<"\n";
            // for(auto dep : inst->getDeps()){
            // cout<<"------------- "<<"dep dyn="<<dep->getDynID()<<" name="<<dep->getOpName()<<"\n";

            // }
            // ++queue_iter;
            break;
        }
        // }
    }

    for (auto queue_iter = reservation.begin(); queue_iter != reservation.end();)
    {
        auto inst = *queue_iter;

        // make sure ld/st insns are in program order
        if ((inst->isStore() || inst->isLoad()) && inst->status == DynOpStatus::INIT)
        {
            // lsu is full and can issue no more ld/st
            if (!lsu->canDispatch(inst, false) || lsu->used_port >= lsu->lsu_port)
            {
                // cout<<"lsu full inst --- "<<inst->getDynID()<<" name="<<inst->getOpName()<<"\n";
                queue_iter++;
                continue;
            }

            // if (inst->status == DynOpStatus::INIT)
            // {
                inst->updateStatus();
                // cout << "add to lsu dynid=" << inst->getDynID() << " op=" << inst->getOpCode() << " name=" << inst->getOpName() << " cycles=" << cycles - temp_acc_cycle << "\n";
                lsu->addInstruction(inst);
                scoreboard.push_back(inst);

                lsu->used_port++;
            // }
        }

        // cout << "process dynid=" << inst->getDynID() << " name=" << inst->getOpName() << " active=" << UIDActive(inst->getUID()) << " ready=" << inst->ready() << "\n";
            // for(auto dep : inst->getDeps()){
            // cout<<"------------- "<<"dep dyn="<<dep->getDynID()<<" name="<<dep->getOpName()<<"\n";

            // }
        if (!UIDActive(inst->getUID()) && inst->ready())
        {
            // cout << "cyle=" << cycles - temp_acc_cycle << " dynid=" << inst->getDynID() << " name=" << inst->getOpName() << " active=" << UIDActive(inst->getUID()) << " ready=" << inst->ready() << " uid=" << inst->getUID() << " status=" << inst->status << "\n";

            if (inst->isLoad() || inst->isStore())
            {
                assert(inst->status == DynOpStatus::ALLOCATED);
            }
            queue_iter = reservation.erase(queue_iter);

            inflightQueue.insert({(inst)->getUID(), inst});
            inst->updateStatus(DynOpStatus::ISSUED);

            inst->execute();
        }
        else
        {
            queue_iter++;
        }
    }
}
#endif

#if 0
void Accelerator::processQueue()
{

    for (auto queue_iter = inflightQueue.begin(); queue_iter != inflightQueue.end();)
    {
        if (commitInsn(queue_iter->second))
        {
            queue_iter = inflightQueue.erase(queue_iter);
            totalCommit++;
        }
        else
        {
            ++queue_iter;
        }
    }

    for (auto queue_iter = reservation.begin(); queue_iter != reservation.end();)
    {
        auto inst = *queue_iter;

        // make sure ld/st insns are in program order
        if (inst->isStore() || inst->isLoad())
        {
            // lsu is full and can issue no more ld/st
            if (!lsu->canDispatch(inst) && inst->status == DynOpStatus::INIT)
            {
                queue_iter++;
                continue;
            }

            if (inst->status == DynOpStatus::INIT)
            {
                inst->updateStatus();
                lsu->addInstruction(inst);
            }
        }

        // cout << "process dynid=" << inst->getDynID() << " name=" << inst->getOpName() << " active=" << UIDActive(inst->getUID()) << " ready=" << inst->ready() << "\n";
        if (!UIDActive(inst->getUID()) && inst->ready())
        {
            inflightQueue.insert({(inst)->getUID(), inst});
            queue_iter = reservation.erase(queue_iter);

            if (inst->isLoad() || inst->isStore())
            {
                assert(inst->status == DynOpStatus::ALLOCATED);
                inst->updateStatus(DynOpStatus::ISSUED);
            }
        }
        else
        {
            queue_iter++;
        }
    }
}
#endif

bool Accelerator::commitInsn(shared_ptr<DynamicOperation> insn)
{
    if (insn->finish())
    {
        // cout << "commit dynid=" << insn->getDynID() << " op=" << insn->getOpCode() << " name=" << insn->getOpName() << " cycles=" << cycles - temp_acc_cycle << "\n";
        // assert(insn->status == DynOpStatus::FINISHED
        insn->signalUser();
        // insn->updateStatus(DynOpStatus::COMMITED);
        return true;
    }
    insn->execute();
    return false;
}

// each time fetches only one BasicBlock
bool Accelerator::fetch()
{
    bool uncond = false;
    vector<shared_ptr<DynamicOperation>> fetchOpBuffer;

    if (reservation.size() >= 1024)
    {
        return false;
    }

    if (fetchForRet)
    {
        // main ret if size == 0
        if (returnStack.size() != 0)
        {
            auto retOps = returnStack.top();
            fetchOpBuffer = retOps->ops;
            bool isInvoke = retOps->isInvoke;
            if (isInvoke)
            {
                if (!task->at_end())
                {
                    string bbname = readBBName();
                }
            }
            returnStack.pop();

            previousBB = prevBBStack.top();
            prevBBStack.pop();
        }
        else
        {
            // Nothing to do, because there is no function calls in accel path
        }
    }
    else
    {
        if (targetBB)
        {
            fetchOpBuffer = scheduleBB(targetBB);
        }
    }
    // targetBB = nullptr;
    fetchForRet = false;
    bool newBBScheduled = false;

    for (int i = 0; i < fetchOpBuffer.size(); i++)
    {
        auto insn = fetchOpBuffer[i];
        // errs() << *(insn->getStaticInsn()->getLLVMInsn()) << "\n";
        insn->updateDynID(globalDynID);
        globalDynID++;
        reservation.push_back(insn);
        findDynamicDeps(insn);

        // insn->setRuntimeCycle(1);

        totalFetch++;

        if (insn->isBr())
        {
            // std::cout << "br inst\n";
            auto br = std::dynamic_pointer_cast<BrDynamicOperation>(insn);
            int cond = true;

            llvm::BranchInst *llvm_br = llvm::dyn_cast<llvm::BranchInst>(insn->getStaticInsn()->getLLVMInsn());
            if (llvm_br->isConditional())
            {
                // when we try to read a value, trace can not be empty
                if (task->at_end())
                {
                    abnormalExit = true;
                    break;
                }
                cond = readBrCondition();
                // cout << cond << "\n";
            }
            else
            {
                uncond = true;
            }

            br->setCondition(cond);
            newBBScheduled = true;
            previousBB = targetBB;
            targetBB = fetchTargetBB(br);

            // control BBs in speculative acc are all uncond branch
            // requiring no other operations
            if (targetBB->getBBName() == "ret.fail")
            {
                failure = true;
            }

            // std::cout << "branch target BB name=" << targetBB->getBBName() << "\n";
            break;
        }
        else if (insn->isCall())
        {
            // std::cout << "call inst\n";
            auto call = std::dynamic_pointer_cast<CallDynamicOperation>(insn);
            previousBB = targetBB;
            targetBB = fetchTargetBB(call);

            // function call is not allowed in accelerator
            // assert(targetBB == nullptr);
            if (targetBB)
            {
                newBBScheduled = true;
                // std::cout << "function target BB name=" << targetBB->getBBName() << "\n";
                std::vector<std::shared_ptr<DynamicOperation>> returnBB(fetchOpBuffer.begin() + i + 1, fetchOpBuffer.begin() + fetchOpBuffer.size());
                returnStack.push(new RetOps(returnBB));
                prevBBStack.push(previousBB);

                fetchOpBuffer.clear();
                break;
            }
        }
        else if (insn->isInvoke())
        {
            auto invoke = std::dynamic_pointer_cast<InvokeDynamicOperation>(insn);
            previousBB = targetBB;

            // this get entry bb of called function
            targetBB = fetchTargetBB(invoke);

            // get normal dest bb
            llvm::Value *destValue = invoke->getDestValue();
            auto normalDestBB = task->fetchBB(destValue);
            newBBScheduled = true;

            if (targetBB)
            {
                // only branch to normal dest
                auto normalDestBBOps = scheduleBB(normalDestBB);

                returnStack.push(new RetOps(normalDestBBOps, true));
                prevBBStack.push(previousBB);

                break;
            }
            else // this function is declartion then invoke is like a br
            {
                if (!task->at_end())
                {
                    if (simpoint)
                    {
                        string bbname = readBBName();
                        // cout << bbname << "\n";
                    }
                }
                targetBB = normalDestBB;
                break;
            }
        }
        else if (insn->isRet())
        {
            // std::cout << "ret inst" << "\n";
            fetchForRet = true;
            break;
        }
        else if (insn->isSwitch())
        {
            if (task->at_end())
            {
                abnormalExit = true;
                break;
            }
            // std::cout << "switch inst\n";
            auto sw = std::dynamic_pointer_cast<SwitchDynamicOperation>(insn);
            int cond = readSwitchCondition();
            sw->setCondition(cond);
            // std::cout << std::hex << cond << "\n";
            // std::cout << std::dec;
            newBBScheduled = true;
            previousBB = targetBB;
            targetBB = fetchTargetBB(sw);
            // std::cout << "switch target BB name=" << targetBB->getBBName() << "\n";
            break;
        }
        else if (insn->isLoad())
        {
            if (task->at_end())
            {
                abnormalExit = true;
                break;
            }
            // std::cout << "load inst\n";
            auto load = std::dynamic_pointer_cast<LoadDynamicOperation>(insn);

            uint64_t addr = readAddr();
            load->setAddr(addr);
            // std::cout << std::hex << addr << "\n";
            // std::cout << std::dec;
        }
        else if (insn->isStore())
        {
            if (task->at_end())
            {
                abnormalExit = true;
                break;
            }
            // std::cout << "store inst\n";
            auto store = std::dynamic_pointer_cast<StoreDynamicOperation>(insn);
            uint64_t addr = readAddr();
            store->setAddr(addr);
            // std::cout << std::hex << addr << "\n";
            // std::cout << std::dec;
        }
        else if (insn->isPhi())
        {
            auto phi = std::dynamic_pointer_cast<PhiDynamicOperation>(insn);
            if (previousBB)
                phi->setPreviousBB(previousBB);
        }
        else
        {
            // std::cout << "arith inst\n";
            insn->execute();
        }
    }

    if (!newBBScheduled || abnormalExit)
        targetBB = nullptr;

    return uncond;
}

void Accelerator::tick()
{
    // in this implementation, each tick only one bb is fetched
    while (fetch())
    {
    }
    // fetch();
    processQueue();

    cycles++;

    if (cycles % 100000 == 0 && offloading)
    {
        cout << "accel cycle=" << cycles << " insns=" << totalCommit << "\n";
    }

    if (isFinish() && offloading)
    {
        offloading = false;
        notifyHostFinish();
    }

    // cout << "q1=" << inflightQueue.size() << " q2=" << reservation.size() << "\n";
}

void Accelerator::switchContext(shared_ptr<SIM::BasicBlock> _targetBB, uint64_t gid)
{
    targetBB = _targetBB;
    globalDynID = gid;

    // init flags
    offloading = true;
    failure = false;

    totalFetch = 0;
    totalCommit = 0;

    temp_acc_cycle = cycles;

    // cout << "offload cycle=" << temp_acc_cycle << "\n";
}

void Accelerator::notifyHostFinish()
{
    Core *host = dynamic_cast<Core *>(hwHost);
    if (!failure)
        accumulateCommit += totalCommit;

    temp_acc_cycle = cycles - temp_acc_cycle;
    // cout << "finish cycle=" << temp_acc_cycle << "\n";
    // cout << "acc exe >>>" << failure << "\n";
    host->switchContext(globalDynID, failure, abnormalExit);
}

void DyserUnit::fetch()
{
    if (!targetBB)
        return;
    std::vector<Instruction *> instructions;
    std::vector<uint64_t> outputInsnUIDs;

    for (auto insn : targetBB->getInstructions())
    {
        Instruction *I = insn->getLLVMInsn();
        // errs()<<*I<<"\n";
        if (insn->getOpName() != "insertvalue" && insn->getOpName() != "ret")
        {
            instructions.push_back(I);
        }
    }

    // find output instructions
    for (auto insn : targetBB->getInstructions())
    {
        Instruction *I = insn->getLLVMInsn();
        if (insn->getOpName() != "insertvalue" && insn->getOpName() != "ret")
        {
            bool isOutput = false;
            int n = 0;
            for (auto &Use : I->uses())
            {
                n++;
                if (Instruction *User = dyn_cast<Instruction>(Use.getUser()))
                {
                    // Check if the user is not in InstructionsToMove
                    if (std::find(instructions.begin(), instructions.end(), User) == instructions.end())
                    {
                        isOutput = true;
                        break;
                    }
                }
            }

            // Add to OutputInstructions if it's used outside of InstructionsToMove and has uses
            if (isOutput && n != 0)
            {
                // errs() << "output " << *I << "\n";
                outputInsnUIDs.push_back(insn->getUID());
            }
        }
    }

    std::set<Value *> insnSet;
    map<uint64_t, vector<uint64_t>> uidInputDynIDMap;

    map<Value *, int> valueIDMap;

    // find input insns
    int inputN = 0;
    for (auto insn : targetBB->getInstructions())
    {
        Instruction *I = insn->getLLVMInsn();
        if (insn->getOpName() != "insertvalue" && insn->getOpName() != "ret")
        {
            vector<uint64_t> dynIDs;
            for (unsigned i = 0; i < I->getNumOperands(); ++i)
            {
                Value *Op = I->getOperand(i);

                if (std::find(instructions.begin(), instructions.end(), Op) == instructions.end())
                {
                    if (isa<Constant>(Op))
                        continue;

                    // errs() << "input " << *I << "\n";

                    if (!valueIDMap.count(Op))
                    {
                        valueIDMap.insert({Op, inputN++});
                    }

                    dynIDs.push_back(valueIDMap[Op]);
                }
            }

            if (dynIDs.size())
            {
                uidInputDynIDMap.insert({insn->getUID(), dynIDs});
            }
        }
    }

    // for (auto item : uidInputDynIDMap)
    // {
    //     auto v = item.second;
    //     cout <<item.first <<"==================== ";
    //     for (auto x : inputDynIDs[v])
    //     {
    //         cout << x << " ";
    //     }
    //     cout << "\n";
    // }

    globalDynID += inputN;

    auto fetchOpBuffer = scheduleBB(targetBB);
    for (int i = 0; i < fetchOpBuffer.size(); i++)
    {
        auto insn = fetchOpBuffer[i];
        if (insn->getOpName() == "insertvalue" || insn->getOpName() == "ret")
            continue;
        // errs() << *(insn->getStaticInsn()->getLLVMInsn()) << "\n";
        insn->updateDynID(globalDynID);
        // insn->setRuntimeCycle(1);
        globalDynID++;
        reservation.push_back(insn);

        uidDynOpMap.insert({insn->getUID(), insn});

        internalFIFOPerInsn.insert({insn, map<uint64_t, int>()});
    }

    // build dynamic dependency
    for (auto insn : reservation)
    {
        // cout << "dynid=" << insn->getDynID() << "\n";
        vector<uint64_t> uids = insn->getStaticDepUIDs();
        for (auto uid : uids)
        {
            if (uidDynOpMap.count(uid))
            {
                auto depOp = uidDynOpMap[uid];

                insn->addDynDeps(depOp->getDynID());
                depOp->addUserDynID(insn->getDynID());

                uint64_t depID = depOp->getDynID();
                internalFIFOPerInsn[insn].insert({depID, 0});

                // cout << "----------------------" << depID << "\n";
            }
        }

        uint64_t uid = insn->getUID();
        if (uidInputDynIDMap.count(uid))
        {
            for (auto inputDynID : uidInputDynIDMap[uid])
            {
                insn->addDynDeps(inputDynID);
                internalFIFOPerInsn[insn].insert({inputDynID, 0});
                // cout << "----------------------" << inputDynID << "\n";
            }
        }
    }

    for (int i = 0; i < outputInsnUIDs.size(); i++)
    {
        auto uid = outputInsnUIDs[i];
        auto op = uidDynOpMap[uid];

        offsetDynIDMap[i] = op->getDynID();
        outputFIFO.insert({op->getDynID(), 0});
    }
}

void DyserUnit::notify(int offset, int stride)
{
    for (int op = offset; op < offset + stride; op++)
    {
        for (auto &item : internalFIFOPerInsn)
        {
            auto &internalFIFO = item.second;
            if (internalFIFO.count(op))
            {
                internalFIFO[op]++;
            }
        }

        // pack call should never touch this
        if (outputFIFO.count(op))
            outputFIFO[op]++;
    }
}

vector<uint64_t> DyserUnit::checkDataReady(shared_ptr<DynamicOperation> insn)
{
    set<uint64_t> ops;
    auto internalFIFO = internalFIFOPerInsn[insn];

    for (auto dep : insn->getDynDeps())
    {
        // cout << "dynid=" << insn->getDynID() << " name=" << insn->getOpName() << " dep = " << dep << "\n";

        assert(internalFIFO.count(dep));
        if (internalFIFO[dep] == 0)
        {
            // cout << "         not ready\n";
            ops.clear();
            break;
        }
        else
        {
            // cout << "         ready\n";
            ops.insert(dep);
        }
    }

    vector<uint64_t> ret(ops.begin(), ops.end());
    return ret;
}

bool DyserUnit::commitInsn(shared_ptr<DynamicOperation> insn)
{
    if (insn->finish())
    {
        // cout << "commit dynid=" << insn->getDynID() << " op=" << insn->getOpCode() << " name=" << insn->getOpName() << " cycles=" << cycles << "\n";
        notify(insn->getDynID(), 1);
        updateInsnCountMap(insn->getOpCode());
        return true;
    }
    insn->execute();
    return false;
}

void DyserUnit::processQueue()
{
    for (auto queue_iter = inflightQueue.begin(); queue_iter != inflightQueue.end();)
    {
        if (commitInsn(queue_iter->second))
        {
            queue_iter = inflightQueue.erase(queue_iter);
        }
        else
        {
            ++queue_iter;
        }
    }

    for (auto insn : reservation)
    {
        // cout << "process dynid=" << insn->getDynID() << " name=" << insn->getOpName() << " active=" << UIDActive(insn->getUID()) << "\n";
        // uid can be dynop id in dyser
        auto ops = checkDataReady(insn);
        if (!UIDActive(insn->getUID()) && ops.size())
        {
            auto &internalFIFO = internalFIFOPerInsn[insn];
            for (auto op : ops)
            {
                assert(internalFIFO.count(op));
                assert(internalFIFO[op] > 0);
                internalFIFO[op]--;
            }
            inflightQueue.insert({(insn)->getUID(), insn});
        }
    }
}

void DyserUnit::tick()
{
    processQueue();

    cycles++;
}

bool DyserUnit::getData(int offset)
{
    uint64_t dynID = offsetDynIDMap[offset];
    assert(outputFIFO.count(dynID));
    if (outputFIFO[dynID] > 0)
    {
        outputFIFO[dynID]--;
        return true;
    }
    return false;
}

void DyserUnit::updateInsnCountMap(int opcode)
{
    if (!insnCountMap.count(opcode))
        insnCountMap[opcode] = 0;

    insnCountMap[opcode] += 1;
}
