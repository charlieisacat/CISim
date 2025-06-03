#pragma once
#ifndef COMMON_H
#define COMMON_H

#include <llvm-c/Core.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include "llvm/Support/raw_ostream.h"

// alias analysis
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Pass.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Passes/PassBuilder.h"

#include <map>

template <typename K, typename V>
void removeItemByValue(std::map<K, V> &myMap, const V &valueToRemove)
{
    // Find the item with the specified value
    auto it = std::find_if(myMap.begin(), myMap.end(), [&valueToRemove](const std::pair<K, V> &item)
                           { return item.second == valueToRemove; });

    // If found, erase it
    if (it != myMap.end())
    {
        myMap.erase(it);
    }
}


#endif