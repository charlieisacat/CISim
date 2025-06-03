#include "power_area_model.h"

PowerAreaModel::PowerAreaModel()
{ // double_multiplier, 0
    fus.push_back(new HWFunctionalUnit(2.253656e+00, 3.942233e+00, 1.402072e-01, 1.576422e+04, 1));
    // bit_register, 1
    fus.push_back(new HWFunctionalUnit(9.743773e-03, 7.400587e-03, 7.395312e-05, 5.981433e+00, 32));
    // bitwise_operations, 2
    fus.push_back(new HWFunctionalUnit(2.013105e-03, 1.583778e-03, 6.111633e-04, 5.036996e+01, 1));
    // double_adder, 3
    fus.push_back(new HWFunctionalUnit(6.272768e-01, 9.240008e-01, 3.505479e-02, 5.116643e+03, 1));
    // float_divider, 4
    fus.push_back(new HWFunctionalUnit(5.817426e-01, 1.041773e+00, 4.215861e-02, 4.905719e+03, 1));
    // bit_shifter, 5
    fus.push_back(new HWFunctionalUnit(1.630555e-02, 6.699894e-02, 1.694823e-03, 2.496461e+02, 1));
    // integer_multiplier, 6
    fus.push_back(new HWFunctionalUnit(6.866739e-01, 1.038409e+00, 4.817683e-02, 4.595000e+03, 1));
    // integer_adder, 7
    fus.push_back(new HWFunctionalUnit(9.743773e-03, 7.400587e-03, 2.380803e-03, 1.794430e+02, 1));
    // double_divider, 8
    fus.push_back(new HWFunctionalUnit(2.253656e+00, 3.942233e+00, 1.402072e-01, 1.576422e+04, 1));
    // float_adder, 9
    fus.push_back(new HWFunctionalUnit(2.420383e-01, 2.865075e-01, 1.520482e-02, 2.063594e+03, 1));
    // float_multiplier, 10
    fus.push_back(new HWFunctionalUnit(5.817426e-01, 1.041773e+00, 4.215861e-02, 4.905719e+03, 1));
}

HWFunctionalUnit *PowerAreaModel::getHWFuncUnit(uint32_t opcode)
{
    if (int_adder_opcodes.count(opcode))
    {
        return fus[7];
    }
    else if (int_mult_opcodes.count(opcode))
    {
        return fus[6];
    }
    else if (fp_adder_opcodes.count(opcode))
    {
        // llvm::Type *type = insn->getType();
        // if (type->isFloatTy())
        // {
        //     return fus[9];
        // }
        // else if (type->isDoubleTy())
        // {
        //     return fus[3];
        // }
        return fus[3];
    }
    else if (fp_mult_opcodes.count(opcode))
    {
        // llvm::Type *type = insn->getType();
        // if (type->isFloatTy())
        // {
        //     return fus[10];
        // }
        // else if (type->isDoubleTy())
        // {
        //     return fus[0];
        // }
        return fus[0];
    }
    else if (bitwise_opcodes.count(opcode))
    {
        return fus[2];
    }

    return nullptr;
}