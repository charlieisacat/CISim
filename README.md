# CISim: ISA-Agnostic Custom Instruction Simulation for General-Purpose Processor

Its DRAM and Cache model are implemented according to https://github.com/PrincetonUniversity/MosaicSim.

# Requirements & Setup
- LLVM-14
- YAML

```bash
sudo apt install llvm-14 clang-14 libyaml-cpp-dev
```

# Compile
## Compile LLVM passes

```bash
cd src/llvm
mkdir build && cd build
cmake ..
make -j
```

## Compile offloader (Custom instruction generator)

```bash
cd src/custom_adaptor
mkdir build && cd build
cmake ..
make -j
```

## Compile

Compile DRAMSim2

```bash
cd src/DRAMSim2
make -j
```

Compile Simulator

```bash
cd src/sim
mkdir build && cd build
cmake ..
make -j
```

## Run
Download MachSuite from https://github.com/charlieisacat/mach_ir

```bash
cd /path/to/mach_ir/aes/aes
make
./aes_run
```

An example.txt will be generated. To run the simulator:

```bash
/path/to/simulator/sim/build/CISim aes_run_rn.bc example.txt /path/to/simulator/sim/ino.yml None None None None 0 0
```

To run custom instructions, please refer to https://github.com/charlieisacat/CIExplorer

## Note
This is an early version of our implementation, which does not fully correspond to the implementation described in the paper ```CISim: ISA-Agnostic Custom Instruction Simulation for General-Purpose Processor```. We will release the correct version of the code as soon as possible.
