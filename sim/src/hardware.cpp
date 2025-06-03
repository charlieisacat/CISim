
#include "hardware.h"

std::string insn_to_string(llvm::Value *v)
{
    std::string str;
    llvm::raw_string_ostream rso(str);
    rso << *v;        // This is where the instruction is serialized to the string.
    return rso.str(); // Return the resulting string.
}

std::shared_ptr<SIM::BasicBlock> Hardware::fetchTargetBB(std::shared_ptr<SwitchDynamicOperation> switchInsn)
{
    if (task->at_end())
        return nullptr;
    // every time we fetch a new bb, read its name first

    string bbname = "";
    if (simpoint)
    {
        bbname = readBBName();
        // cout << bbname << "\n";
    }

    int cond = switchInsn->getCondition();
    // std::cout << cond << "\n";
    llvm::Value *destValue = switchInsn->getDestValue(cond);

    auto ret = task->fetchBB(destValue);
    return ret;
}

std::shared_ptr<SIM::BasicBlock> Hardware::fetchTargetBB()
{
    if (task->at_end())
        return nullptr;
    string bbname = readBBName();
    // cout << bbname << "\n";
    llvm::Value *destValue = task->name_bb_map[bbname];
    // assert(destValue);
    if (!destValue)
        return nullptr;

    auto ret = task->fetchBB(destValue);
    return ret;
}

std::shared_ptr<SIM::BasicBlock> Hardware::fetchTargetBB(std::shared_ptr<BrDynamicOperation> br)
{
    if (task->at_end())
        return nullptr;

    int cond = br->getCondition();
    // std::cout << cond << "\n";
    llvm::Value *destValue = br->getDestValue(cond);

    std::shared_ptr<SIM::BasicBlock> ret = task->fetchBB(destValue);
    string fetchedBBName = ret->getBBName();
    // cout << "fetchedBBname=" << fetchedBBName << "\n";

    string bbname = "";
    if (simpoint && fetchedBBName[0] != 'g' && fetchedBBName != "ret.fail" && fetchedBBName != "offload.true" && fetchedBBName != "offload.false" && fetchedBBName != "offload" && fetchedBBName != "mergeblock")
    {
        bbname = readBBName();
        // cout << bbname << "\n";
    }

    return ret;
}

std::shared_ptr<SIM::BasicBlock> Hardware::fetchTargetBB(std::shared_ptr<InvokeDynamicOperation> invoke)
{
    if (task->at_end())
        return nullptr;

    if (invoke->hasBody)
    {
        string bbname = "";
        if (simpoint)
        {
            bbname = readBBName();
            // cout << bbname << "\n";
        }
        auto sfunc = task->fetchFunc(invoke->getCalledFunc());

        auto ret = sfunc->getEntryBB();
        return ret;
    }

    // it is a function ptr
    auto func = invoke->getCalledFunc();
    if (!func)
    {
        // TODO: fix: only work with simpoint now
        return fetchTargetBB();
    }

    return nullptr;
}

std::shared_ptr<SIM::BasicBlock> Hardware::fetchTargetBB(std::shared_ptr<CallDynamicOperation> call)
{
    if (task->at_end())
        return nullptr;

    if (call->hasBody)
    {
        string bbname = "";
        if (simpoint)
        {
            bbname = readBBName();
            // cout << bbname << "\n";
        }
        // cout<<"????? "<<call->getCalledFunc()->getName().str()<<"\n";
        auto sfunc = task->fetchFunc(call->getCalledFunc());
        auto ret = sfunc->getEntryBB();
        return ret;
    }

    // it is a function ptr
    auto func = call->getCalledFunc();
    if (!func && call->shouldRead())
    {
        // TODO: fix: only work with simpoint now
        return fetchTargetBB();
    }
    return nullptr;
}

// convert static insns to dynamic insns
std::vector<std::shared_ptr<DynamicOperation>> Hardware::scheduleBB(std::shared_ptr<SIM::BasicBlock> sbb)
{
    std::vector<std::shared_ptr<DynamicOperation>> ret;
    // can't read trace here,
    // it may jump to other bbs in the middle because of call and then read wrong trace
    for (auto static_insn : sbb->getInstructions())
    {
        uint32_t opcode = static_insn->getOpCode();
        auto opname = static_insn->getOpName();
        // std::cout << "opname=" << opname << " opcode=" << opcode << "\n";
        // cout << insn_to_string(static_insn->getLLVMInsn()) << "\n";

        // assert(opname != "freeze");
        assert(opname != "unreachable");
        assert(opname != "landingpad");
        assert(opname != "resume");
        assert(opname != "indirectbr");

        assert(params->insnConfigs.count(opcode));

        switch (opcode)
        {
        case llvm::Instruction::Ret:
        {
            auto dyn_op = std::make_shared<RetDynamicOperation>(static_insn, params->insnConfigs[opcode]);
            ret.push_back(dyn_op);
            break;
        }
        case llvm::Instruction::Br:
        {
            auto dyn_op = std::make_shared<BrDynamicOperation>(static_insn, params->insnConfigs[opcode]);
            ret.push_back(dyn_op);
            break;
        }
        case llvm::Instruction::Invoke:
        {
            auto dyn_op = std::make_shared<InvokeDynamicOperation>(static_insn, params->insnConfigs[opcode]);
            ret.push_back(dyn_op);
            break;
        }
        case llvm::Instruction::Load:
        {
            auto dyn_op = std::make_shared<LoadDynamicOperation>(static_insn, params->insnConfigs[opcode]);
            ret.push_back(dyn_op);
            break;
        }
        case llvm::Instruction::Store:
        {
            auto dyn_op = std::make_shared<StoreDynamicOperation>(static_insn, params->insnConfigs[opcode]);
            ret.push_back(dyn_op);
            break;
        }
        case llvm::Instruction::Call:
        {
            assert(params->insnConfigs.count(opcode));
            auto dyn_op = std::make_shared<CallDynamicOperation>(static_insn, params->insnConfigs[opcode]);
            ret.push_back(dyn_op);
            if (dyn_op->isEncapsulateFunc || dyn_op->isPackingFunc || dyn_op->isExtractFunc)
            {
                stat->updateCustomInsnCount(dyn_op);
                dyn_op->updatePortMapping(params->insnConfigs, percentage);
            }

            if (dyn_op->isPackingFunc)
            {
                stat->packing_func_num++;
            }

            if (dyn_op->isEncapsulateFunc)
            {
                bool updateArea = stat->shouldUpdateArea(dyn_op);
                bool updatePower = stat->shouldUpdatePower(dyn_op);

                if ((updateArea == false && updatePower == true) || (updateArea == true && updatePower == false))
                    assert(0);

                // compute area
                if (updateArea && updatePower)
                {
                    double area = 0.;
                    double power = 0.;
                    for (auto op : dyn_op->fuOps)
                    {
                        auto hwfu = pam->getHWFuncUnit(op);
                        if (hwfu)
                        {
                            area += hwfu->area;
                            power += hwfu->internal_power + hwfu->switch_power;
                        }
                        else
                        {
                            std::cout << "not concern, opcode=" << op << "\n";
                        }
                    }

                    stat->updateCustomInsnPower(dyn_op, power);
                    stat->updateCustomInsnArea(dyn_op, area);
                }
            }

            break;
        }
        case llvm::Instruction::Switch:
        {
            auto dyn_op = std::make_shared<SwitchDynamicOperation>(static_insn, params->insnConfigs[opcode]);
            ret.push_back(dyn_op);
            break;
        }
        case llvm::Instruction::PHI:
        {
            auto dyn_op = std::make_shared<PhiDynamicOperation>(static_insn, params->insnConfigs[opcode]);
            ret.push_back(dyn_op);
            break;
        }
        default:
        {
            auto dyn_op = std::make_shared<ArithDynamicOperation>(static_insn, params->insnConfigs[opcode]);
            ret.push_back(dyn_op);

            if (dyn_op->getOpName() == "extractvalue")
            {
                stat->extract_insn_num++;
            }
            break;
        }
        }
    }
    return ret;
}