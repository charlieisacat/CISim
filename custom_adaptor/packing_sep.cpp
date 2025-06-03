#include "custom_adaptor.h"
#include <iostream>
#include <string>
#include <filesystem>
#include <stack>
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

#include "yaml-cpp/yaml.h"

using namespace llvm;

class OrderedInsnSet
{
public:
    void insert(Instruction *insn)
    {
        if (set.find(insn) == set.end())
        { // Only insert if not already present
            set.insert(insn);
            order.push_back(insn);
        }
    }

    std::unordered_set<Instruction *> set; // To check for duplicates
    std::vector<Instruction *> order;      // To maintain order
};

void readBBList(std::string filename, std::vector<std::string> &bbLists)
{
    std::fstream file;

    if (filename.empty())
    {
        std::cout << "No bbList specified - \n";
        return;
    }
    else
        file.open(filename);

    std::string bb_name;
    while (file >> bb_name)
    {
        std::cout << "reading bbname=" << bb_name << "\n";
        bbLists.push_back(bb_name);
    }
}

bool isLaterInBlock(Instruction *A, Instruction *B)
{
    if (!B)
        return true;

    // Both instructions must be in the same basic block.
    if (A->getParent() != B->getParent())
    {
        return false;
    }

    // Iterate through the instructions in the block until you find B.
    for (auto &I : *A->getParent())
    {
        if (&I == A)
        {
            return false; // A appears first or they are the same instruction.
        }
        if (&I == B)
        {
            return true; // B appears first, so A is later.
        }
    }
    return false; // A was not found, which should not happen if both are in the same block.
}

void moveInstructionAfter(Instruction *I, Instruction *TargetInst)
{
    // errs()<<"move from"<<*I<<" to"<<*TargetInst<<"\n";
    // Remove the instruction from its current position
    I->removeFromParent();

    // Insert the instruction after the target instruction
    I->insertAfter(TargetInst);

    for (auto UI = I->use_begin(), UE = I->use_end(); UI != UE;)
    {
        Use &U = *UI++; // Increment iterator before using, to safely remove/replace
        Instruction *User = dyn_cast<Instruction>(U.getUser());

        // Check if the user is outside the new function
        if (User)
        {
            if (User->getParent()->getName() == I->getParent()->getName() && !isa<PHINode>(User))
            {
                if (!isLaterInBlock(User, I))
                {
                    // errs() << "       User=" << *User << "\n";
                    moveInstructionAfter(User, I);
                }
            }
        }
    }
}

bool hasDependencyEndingInVector(Instruction *inst, std::set<Instruction *> instSet)
{
    std::stack<Instruction *> worklist;
    std::set<Instruction *> visited;

    worklist.push(inst);

    while (!worklist.empty())
    {
        Instruction *current = worklist.top();
        worklist.pop();

        if (visited.count(current))
            continue;
        visited.insert(current);

        // If current is within the vector, dependency ends within the vector
        if (instSet.count(current))
        {
            return true;
        }

        // Continue exploring operands of current instruction
        for (Use &U : current->operands())
        {
            if (Instruction *operandInst = dyn_cast<Instruction>(U.get()))
            {
                if (!visited.count(operandInst))
                {
                    worklist.push(operandInst);
                }
            }
        }
    }

    return false;
}

// return true if non-convex
bool dfsCheckConvex(std::set<Instruction *> instructionSet, Instruction *insn)
{
    bool ret = false;
    for (Use &U : insn->operands())
    {
        if (Instruction *operandInst = dyn_cast<Instruction>(U.get()))
        {
            if (!instructionSet.count(operandInst))
            {
                // If operand is outside the vector, check if the dependency chain ends in the vector
                if (!isa<PHINode>(operandInst) && hasDependencyEndingInVector(operandInst, instructionSet))
                {
                    return true; // Found an external dependency that ends in the vector
                }
            }
            else
            {
                ret |= dfsCheckConvex(instructionSet, operandInst);
            }
        }
    }

    return ret; // No problematic dependencies found
}
bool isInAnotherPattern(Value *Op, std::vector<Instruction *> another, std::string bbName)
{

    if (Instruction *depInst = dyn_cast<Instruction>(Op))
    {
        if (bbName == depInst->getParent()->getName())
        {
            if (std::find(another.begin(), another.end(), depInst) != another.end())
            {
                return true;
            }
        }
    }

    return false;
}

bool checkCycle(std::vector<Instruction *> v1, std::vector<Instruction *> v2)
{
    bool b1 = false;
    bool b2 = false;

    for (auto I : v1)
    {
        for (unsigned i = 0; i < I->getNumOperands(); ++i)
        {
            Value *Op = I->getOperand(i);
            if (isa<Constant>(Op))
                continue;

            if (isInAnotherPattern(Op, v2, I->getParent()->getName().str()))
            {
                b1 = true;
                break;
            }
        }
    }

    for (auto I : v2)
    {
        for (unsigned i = 0; i < I->getNumOperands(); ++i)
        {
            Value *Op = I->getOperand(i);
            if (isa<Constant>(Op))
                continue;
            if (isInAnotherPattern(Op, v1, I->getParent()->getName().str()))
            {
                b2 = true;
                break;
            }
        }
    }
    return b1 & b2;
}

// avoid enca function cycle
Candidate removeCycle(Candidate candidate, std::vector<Instruction *> instructions)
{
    Candidate ret;
    int size = candidate.size();
    std::vector<std::vector<Instruction *>> prevInsns;

    for (int i = 0; i < size; i++)
    {
        auto component = candidate[i];
        std::vector<Instruction *> tmpInsns;
        for (auto v : component->vertices)
        {
            tmpInsns.push_back(instructions[v]);
        }

        bool cycle = false;
        for (int j = 0; j < prevInsns.size(); j++)
        {
            if (checkCycle(prevInsns[j], tmpInsns))
            {
                cycle = true;
                break;
            }
        }

        if (!cycle)
        {
            prevInsns.push_back(tmpInsns);
            ret.push_back(component);
        }
    }

    return ret;
}

void sortCandidate(
    std::vector<std::string> bbLists,
    Candidate &candidate)
{
    // Create a map from bbName to its index in bbLists
    std::unordered_map<std::string, size_t> bbIndexMap;
    for (size_t i = 0; i < bbLists.size(); ++i)
    {
        bbIndexMap[bbLists[i]] = i;
    }

    // Sort the candidate vector based on the order in bbLists
    std::sort(candidate.begin(), candidate.end(),
              [&bbIndexMap](CandidateComponent *a, CandidateComponent *b)
              {
                  // Get the index of the bbName in bbLists using the map
                  size_t indexA = bbIndexMap[a->bbName];
                  size_t indexB = bbIndexMap[b->bbName];
                  return indexA < indexB;
              });
}

void customize(int opcode,
               Candidate candidate,
               std::vector<Instruction *> instructions,
               Module &M,
               LLVMContext &Context,
               unsigned int Threshold)
{
    DataLayout DL(&M); // Data layout to compute sizes
    // Declare malloc function in the module
    FunctionType *MallocFuncType = FunctionType::get(Type::getInt8PtrTy(Context), {Type::getInt64Ty(Context)}, false);
    FunctionCallee MallocFunc = M.getOrInsertFunction("malloc", MallocFuncType);

    auto ret = removeCycle(candidate, instructions);

    for (auto component : ret)
    {
        std::vector<Instruction *> InstructionsToMove;
        std::set<Value *> OutputsInNewFunction;
        // std::set<Instruction *> OutputInstructions;
        OrderedInsnSet OutputInstructions;

        std::sort(component->vertices.begin(), component->vertices.end());

        for (auto v : component->vertices)
        {
            InstructionsToMove.push_back(instructions[v]);
            OutputsInNewFunction.insert(instructions[v]); // Track outputs within the new function
        }

        BasicBlock *BB = InstructionsToMove[0]->getParent();

        std::set<Instruction *> instructionSet(InstructionsToMove.begin(), InstructionsToMove.end());

        bool nonConvex = false;
        for (auto I : InstructionsToMove)
            nonConvex |= dfsCheckConvex(instructionSet, I);
        // errs() << "convex=" << !nonConvex << "\n";
        if (nonConvex)
            continue;

        // Identify output instructions (those not used within InstructionsToMove)
        for (Instruction *I : InstructionsToMove)
        {
            // errs() << "move :" << *I << "\n";
            bool isOutput = false; // Assume it's not an output until proven otherwise
            int n = 0;

            for (auto &Use : I->uses())
            {
                n++;
                if (Instruction *User = dyn_cast<Instruction>(Use.getUser()))
                {
                    // Check if the user is not in InstructionsToMove
                    if (std::find(InstructionsToMove.begin(), InstructionsToMove.end(), User) == InstructionsToMove.end())
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
                OutputInstructions.insert(I);
            }
        }

        // Determine the types of inputs and outputs
        std::vector<Type *> InputTypes;
        std::vector<Value *> Args;
        std::set<Value *> ArgsSet; // To ensure we don't add the same argument multiple times

        Instruction *lastDep = nullptr;
        // Add function arguments (inputs) and map them to the new function
        for (Instruction *I : InstructionsToMove)
        {
            for (unsigned i = 0; i < I->getNumOperands(); ++i)
            {
                Value *Op = I->getOperand(i);
                // If the operand is not produced by another instruction that will be moved,
                // and it is not already in ArgsSet, add it as an input argument
                if (OutputsInNewFunction.find(Op) == OutputsInNewFunction.end() &&
                    ArgsSet.find(Op) == ArgsSet.end())
                {
                    if (isa<Constant>(Op))
                        continue;

                    InputTypes.push_back(Op->getType());
                    Args.push_back(Op);
                    ArgsSet.insert(Op); // Track added arguments

                    if (Instruction *depInst = dyn_cast<Instruction>(Op))
                    {
                        if (I->getParent()->getName() == depInst->getParent()->getName())
                        {
                            if (isLaterInBlock(depInst, lastDep))
                            {
                                lastDep = depInst;
                            }
                            // llvm::errs() << "lastDep=" << *lastDep << "\n";
                        }
                    }
                }
            }
        }

        // Split the inputs into multiple structs based on the threshold
        unsigned NumStructs = (Args.size() + Threshold - 1) / Threshold;
        std::vector<StructType *> StructTypes;
        std::vector<Value *> StructArgs;

        for (unsigned i = 0; i < NumStructs; ++i)
        {
            unsigned StartIdx = i * Threshold;
            unsigned EndIdx = std::min(StartIdx + Threshold, (unsigned)Args.size());

            std::vector<Type *> SubInputTypes(InputTypes.begin() + StartIdx, InputTypes.begin() + EndIdx);
            StructType *InputStructType = StructType::create(Context, SubInputTypes, "InputStruct_" + std::to_string(i));
            StructTypes.push_back(InputStructType);
        }

        // Create the return type as a structure that includes all outputs
        std::vector<Type *> OutputTypes;
        for (Instruction *I : OutputInstructions.order)
        {
            OutputTypes.push_back(I->getType());
        }

        bool noRequirePacking = false;
        if (Args.size() + OutputTypes.size() <= 4)
        {
            // llvm::errs() << "no packing\n";
            noRequirePacking = true;
        }

        bool singleRet = false;
        if (OutputTypes.size() == 1)
        {
            // llvm::errs() << "single ret\n";
            singleRet = true;
        }

        // Define the new encapsulated function type (this will use multiple structs as arguments)
        std::vector<Type *> EncapsFuncArgs;

        if (noRequirePacking)
        {
            EncapsFuncArgs = InputTypes;
        }
        else
        {
            for (auto *StructType : StructTypes)
            {
                EncapsFuncArgs.push_back(StructType->getPointerTo());
            }
        }
        FunctionType *EncapsFuncType = nullptr;
        Type *ReturnType = nullptr;

        if (singleRet)
        {
            EncapsFuncType = FunctionType::get(OutputTypes[0], EncapsFuncArgs, false);
        }
        else
        {
            ReturnType = StructType::get(Context, OutputTypes);
            EncapsFuncType = FunctionType::get(ReturnType, EncapsFuncArgs, false);
        }

        // Create the new encapsulated function
        Function *EncapsFunc = Function::Create(EncapsFuncType, Function::InternalLinkage, "encapsulated_function_" + BB->getName(), &M);
        BasicBlock *EncapsBB = BasicBlock::Create(Context, "entry", EncapsFunc);
        IRBuilder<> EncapsBuilder(EncapsBB);

        // Add metadata to indicate this is an encapsulated function
        std::vector<Metadata *> Values;
        Values.push_back(MDString::get(Context, std::to_string(opcode)));
        Values.push_back(MDString::get(Context, component->cpStr));
        Values.push_back(MDString::get(Context, component->fuStr));
        MDNode *ComplexMetaNode = MDNode::get(Context, Values);
        EncapsFunc->setMetadata(EncapsFunc->getName().str(), ComplexMetaNode);

        // Map function arguments to the original operands in the encapsulated instructions
        auto ArgIt = EncapsFunc->arg_begin();
        std::map<Value *, Value *> ArgToValMap;
        std::vector<Value *> StructPointers;

        if (noRequirePacking)
        {
            for (Value *Arg : Args)
            {
                ArgIt->setName(Arg->getName());
                ArgToValMap[Arg] = &*ArgIt; // Map the argument in the new function to the original operand
                ++ArgIt;
            }
        }
        else
        {
            for (unsigned i = 0; i < NumStructs; ++i)
            {
                StructPointers.push_back(&*ArgIt++);
            }

            for (unsigned i = 0; i < StructPointers.size(); ++i)
            {
                Value *LoadedStruct = EncapsBuilder.CreateLoad(StructTypes[i], StructPointers[i], "loaded_struct_" + std::to_string(i));

                unsigned StartIdx = i * Threshold;
                unsigned EndIdx = std::min(StartIdx + Threshold, (unsigned)Args.size());

                for (unsigned j = StartIdx; j < EndIdx; ++j)
                {
                    Value *ExtractedValue = EncapsBuilder.CreateExtractValue(LoadedStruct, {j - StartIdx}, "extracted_" + std::to_string(j));
                    ArgToValMap[Args[j]] = ExtractedValue;
                }
            }
        }

        // Update the operands of instructions to point to the extracted values from the structs
        for (Instruction *I : InstructionsToMove)
        {
            for (unsigned i = 0; i < I->getNumOperands(); ++i)
            {
                Value *Op = I->getOperand(i);
                if (ArgToValMap.find(Op) != ArgToValMap.end())
                {
                    I->setOperand(i, ArgToValMap[Op]); // Replace operand with argument in new function
                }
            }
        }

        std::vector<Function *> PackingFuncs;
        if (!noRequirePacking)
        {
            // Create multiple packing functions, one for each struct
            for (unsigned i = 0; i < StructTypes.size(); ++i)
            {
                unsigned StartIdx = i * Threshold;
                unsigned EndIdx = std::min(StartIdx + Threshold, (unsigned)Args.size());

                std::vector<Type *> PackingFuncInputTypes(InputTypes.begin() + StartIdx, InputTypes.begin() + EndIdx);
                if (i > 0)
                {
                    PackingFuncInputTypes.push_back(StructTypes[i - 1]->getPointerTo());
                }
                FunctionType *PackingFuncType = FunctionType::get(StructTypes[i]->getPointerTo(), PackingFuncInputTypes, false);

                // Create the packing function
                Function *PackingFunc = Function::Create(PackingFuncType, Function::InternalLinkage, "packing_function_" + std::to_string(i), &M);
                BasicBlock *PackingBB = BasicBlock::Create(Context, "entry", PackingFunc);
                IRBuilder<> PackingBuilder(PackingBB);

                // Add metadata to indicate this is a packing function
                MDNode *MetadataNode = MDNode::get(Context, MDString::get(Context, std::to_string(opcode)));
                PackingFunc->setMetadata("packing_func", MetadataNode);

                // Allocate memory for the struct using malloc
                auto IntPtrTy = Type::getInt64Ty(Context); // 64-bit pointer size
                Value *SizeOfStruct = ConstantInt::get(IntPtrTy, DL.getTypeAllocSize(StructTypes[i]));
                Value *InputStructPtrInPackingFunc = PackingBuilder.CreateCall(MallocFunc, {SizeOfStruct});
                InputStructPtrInPackingFunc = PackingBuilder.CreateBitCast(InputStructPtrInPackingFunc, StructTypes[i]->getPointerTo(), "input_struct_ptr");

                Value *InputStruct = UndefValue::get(StructTypes[i]);

                int TypeSize = PackingFuncInputTypes.size();
                if (i > 0)
                {
                    TypeSize -= 1;
                }
                for (unsigned j = 0; j < TypeSize; ++j)
                {
                    InputStruct = PackingBuilder.CreateInsertValue(InputStruct, PackingFunc->getArg(j), j);
                }

                PackingBuilder.CreateStore(InputStruct, InputStructPtrInPackingFunc);
                PackingBuilder.CreateRet(InputStructPtrInPackingFunc);

                PackingFuncs.push_back(PackingFunc);
            }
        }

        // Replace the original instructions with calls to packing functions and encapsulated function
        IRBuilder<> Builder(Context);

        if (lastDep)
        {
            auto iter = lastDep->getIterator();
            while (isa<PHINode>(iter))
            {
                iter++;
            }
            Builder.SetInsertPoint(lastDep->getParent(), ++iter);
        }
        else
        {
            auto inst = InstructionsToMove.back();
            Builder.SetInsertPoint(inst->getParent(), inst->getIterator());
        }

        CallInst *Call = nullptr;
        if (!noRequirePacking)
        {
            std::vector<Value *> PackedStructs;
            for (unsigned i = 0; i < PackingFuncs.size(); ++i)
            {
                unsigned StartIdx = i * Threshold;
                unsigned EndIdx = std::min(StartIdx + Threshold, (unsigned)Args.size());

                std::vector<Value *> PackingFuncArgs(Args.begin() + StartIdx, Args.begin() + EndIdx);
                if (i > 0)
                {
                    PackingFuncArgs.push_back(PackedStructs[i - 1]);
                }
                PackedStructs.push_back(Builder.CreateCall(PackingFuncs[i], PackingFuncArgs));
            }
            Call = Builder.CreateCall(EncapsFunc, PackedStructs, "call_func");
        }
        else
        {
            Call = Builder.CreateCall(EncapsFunc, Args, "call_func");
        }

        // Create a call to the encapsulated function with the struct pointers

        std::vector<Instruction *> suffixInsns;
        std::map<Instruction *, Instruction *> targetMap;

        unsigned Index = 0;
        for (Instruction *I : OutputInstructions.order)
        {
            Value *ExtractedValue = nullptr;
            if (!singleRet)
                ExtractedValue = Builder.CreateExtractValue(Call, Index++, I->getName() + ".extracted");
            // Iterate over the uses of the instruction
            for (auto UI = I->use_begin(), UE = I->use_end(); UI != UE;)
            {
                Use &U = *UI++; // Increment iterator before using, to safely remove/replace
                Instruction *User = dyn_cast<Instruction>(U.getUser());

                // Check if the user is outside the new function
                if (User && User->getFunction() != EncapsFunc)
                {
                    if (std::find(InstructionsToMove.begin(), InstructionsToMove.end(), User) == InstructionsToMove.end())
                    {
                        // llvm::errs() << *User << "\n";
                        // llvm::errs() << "User->get=" << User->getFunction()->getName().str() << " " << EncapsFunc->getName().str() << "\n";
                        // llvm::errs() << "Replacing use in: " << *User << "\n";
                        if (!singleRet)
                        {
                            U.set(ExtractedValue); // Replace the use with the extracted value
                            Instruction *tmp = dyn_cast<Instruction>(ExtractedValue);
                            if (!isLaterInBlock(User, tmp))
                            {
                                // suffixInsns.push_back(User);
                                // if (targetMap.find(User) != targetMap.end())
                                // {
                                //     assert(targetMap[User] != tmp);
                                // }
                                // targetMap.insert(std::make_pair(User, tmp));
                                if (!isa<PHINode>(User) && User->getParent()->getName() == tmp->getParent()->getName())
                                    moveInstructionAfter(User, tmp);
                            }
                        }
                        else
                        {
                            U.set(Call); // Replace the use with the extracted value
                            if (!isLaterInBlock(User, Call))
                            {
                                // suffixInsns.push_back(User);
                                // if (targetMap.find(User) != targetMap.end())
                                // {
                                //     assert(targetMap[User] != Call);
                                // }
                                // targetMap.insert(std::make_pair(User, Call));
                                if (!isa<PHINode>(User) && User->getParent()->getName() == Call->getParent()->getName())
                                    moveInstructionAfter(User, Call);
                            }
                        }
                    }
                }
                else
                {
                    // llvm::errs() << "Use inside new function, not replacing: " << *User << "\n";
                }
            }
        }

        // for (auto I : suffixInsns)
        // {
        //     // errs() << "suffix insn=" << *I << "\n";
        //     if (!isa<PHINode>(I) && I->getParent()->getName() == targetMap[I]->getParent()->getName())
        //         moveInstructionAfter(I, targetMap[I]);
        // }

        for (Instruction *I : InstructionsToMove)
        {
            I->removeFromParent();   // Remove the instruction from its original location
            EncapsBuilder.Insert(I); // Insert the instruction into the new function
        }

        // Create a struct to hold the return values
        if (!singleRet)
        {
            Value *ReturnStruct = UndefValue::get(ReturnType);
            Index = 0;
            for (Instruction *I : OutputInstructions.order)
            {
                ReturnStruct = EncapsBuilder.CreateInsertValue(ReturnStruct, I, Index++);
            }
            EncapsBuilder.CreateRet(ReturnStruct);
        }
        else
        {
            EncapsBuilder.CreateRet(*OutputInstructions.order.begin());
        }

        // Verify the module
        // if (llvm::verifyModule(M, &llvm::errs()))
        // {
        //     llvm::errs() << "Error: Component verification failed!\n";
        //     return {}; // Return a non-zero value to indicate failure
        // }
    }
}

int main(int argc, char **argv)
{

    assert(argc == 9);
    std::string irPath = std::string(argv[1]);
    std::string patternDir = std::string(argv[2]);
    std::string cpOpsDir = std::string(argv[3]);
    std::string fuOpsDir = std::string(argv[4]);
    unsigned int Threshold = std::stoul(argv[5]); // Set the threshold for the maximum number of inputs per struct
    std::string bbListPath = std::string(argv[6]);
    std::string output = std::string(argv[7]);
    int threshold = std::stoi(argv[8]);

    LLVMContext Context;
    SMDiagnostic Err;

    std::vector<std::string> bbList;
    readBBList(bbListPath, bbList);

    int n = 0;
    // errs() << patternDir << "\n";

    std::vector<Candidate> candidates;

    // Get all file names in the first directory
    for (const auto &entry : fs::directory_iterator(patternDir))
    {
        if (entry.is_regular_file())
        {
            int opcode = 1025;

            // Load the LLVM module from a file
            std::unique_ptr<Module> M = parseIRFile(irPath, Err, Context);

            uint64_t uid = 0;
            std::vector<Instruction *> instructions;
            for (auto &F : *M)
            {
                for (auto &BB : F)
                {
                    for (auto &I : BB)
                    {
                        instructions.push_back(&I);
                        uid++;
                    }
                }
            }

            std::string filename = entry.path().filename().string();
            std::cout << "filename=" << filename << " opcode=" << opcode << "\n";
            std::string patternPath = patternDir + "/" + filename;
            std::string cpOpsPath = cpOpsDir + "/" + filename;
            std::string fuOpsPath = fuOpsDir + "/" + filename;

            Candidate candidate = readCustomInsn(patternPath);
            sortCandidate(bbList, candidate);

            std::vector<std::vector<uint64_t>> cpOps = readCriticalPathOps(cpOpsPath);
            std::vector<std::vector<uint64_t>> fuOps = readCriticalPathOps(fuOpsPath);
            assert(cpOps.size() == candidate.size());

            // build a string for all ops
            std::string fuStr = "";
            for (int j = 0; j < fuOps[0].size(); j++)
            {
                fuStr += std::to_string(fuOps[0][j]);
                if (j != fuOps[0].size() - 1)
                {
                    fuStr += ",";
                }
            }

            for (int i = 0; i < candidate.size(); i++)
            {
                auto compnt = candidate[i];
                compnt->fuStr = fuStr;

                // build a string for ops on critical path
                std::vector<uint64_t> ops = cpOps[i];
                std::string cpStr = "";
                for (int j = 0; j < ops.size(); j++)
                {
                    uint64_t vid = ops[j];
                    cpStr += std::to_string(instructions[vid]->getOpcode());
                    if (j != ops.size() - 1)
                    {
                        cpStr += ",";
                    }
                }

                compnt->cpStr = cpStr;
                compnt->opcode = opcode;
            }
            if (!candidate.size())
                continue;

            // std::cout<<"opcode="<<opcode<<"\n";
            customize(opcode, candidate, instructions, *M, Context, Threshold);

            // Write the modified module to a file
            std::error_code EC;
            raw_fd_ostream OS(output + "_" + std::to_string(n) + ".ll", EC, sys::fs::OF_None);
            if (EC)
            {
                errs() << "Could not open file: " << EC.message();
                return 1;
            }

            M->print(OS, nullptr);
            OS.flush();

            // Verify the module
            if (llvm::verifyModule(*M, &llvm::errs()))
            {
                llvm::errs() << "Error: Module verification failed!\n";
                return 1; // Return a non-zero value to indicate failure
            }

            YAML::Emitter out;

            // custom port
            out << YAML::BeginMap; // Begin a new map
            out << YAML::Key << "ports";
            out << YAML::BeginMap;

            // one for packing, one for extract
            for (int i = 0; i < 3; i++)
            {
                std::string port = "Port" + std::to_string(i);
                // out << YAML::Key << port << YAML::Value << i;
                out << YAML::Key << port;
                out << YAML::BeginMap;
                out << YAML::Key << "id" << YAML::Value << i;
                out << YAML::EndMap;
            }

            out << YAML::EndMap;
            // custom insn
            out << YAML::Key << "instructions";

            out << YAML::BeginMap;
            out << YAML::Key << "packing";
            out << YAML::BeginMap;
            out << YAML::Key << "ports" << YAML::Value << 0;
            out << YAML::Key << "fu" << YAML::Value << 0;
            out << YAML::Key << "opcode_num" << YAML::Value << 1023;
            out << YAML::Key << "runtime_cycles" << YAML::Value << 1;
            out << YAML::EndMap;

            out << YAML::Key << "extract";
            out << YAML::BeginMap;
            out << YAML::Key << "ports" << YAML::Value << 1;
            out << YAML::Key << "fu" << YAML::Value << 1;
            out << YAML::Key << "opcode_num" << YAML::Value << 1024;
            out << YAML::Key << "runtime_cycles" << YAML::Value << 1;
            out << YAML::EndMap;

            std::string customInsn = "custom_insn" + std::to_string(0);
            std::cout << "customInsn = " << customInsn << " ???" << "\n";
            out << YAML::Key << customInsn;
            out << YAML::BeginMap;
            out << YAML::Key << "ports" << YAML::Value << 2;
            out << YAML::Key << "fu" << YAML::Value << 2;
            out << YAML::Key << "opcode_num" << YAML::Value << 1025;
            out << YAML::Key << "runtime_cycles" << YAML::Value << 0;
            out << YAML::EndMap;

            out << YAML::EndMap;
            out << YAML::EndMap;

            // Output to console
            std::cout << "Generated YAML:\n"
                      << out.c_str() << std::endl;

            // Write to file
            std::ofstream fout(output + "_" + std::to_string(n) + ".yaml");
            fout << out.c_str();
            fout.close();

            n++;
        }
    }

    return 0;
}
