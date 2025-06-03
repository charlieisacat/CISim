#ifndef VALUE_H
#define VALUE_H

#include <map>
#include <unordered_map>
namespace SIM
{
    class Value
    {
    public:
        Value(uint64_t id) : uid(id) {}
        virtual void initialize(std::unordered_map<llvm::Value *, std::shared_ptr<SIM::Value>> value_map) {}

        uint64_t getUID() { return uid; }
    protected:
        uint64_t uid;
    };
}
#endif