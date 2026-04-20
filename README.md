# CISim: ISA-Agnostic Custom Instruction Simulation for General-Purpose Processor

This is the simulator we used in CIExplorer https://github.com/charlieisacat/CIExplorer.

Its DRAM and Cache model are implemented according to https://github.com/PrincetonUniversity/MosaicSim.

## Citation
> Xiaoyu Hao, Sen Zhang, Liang Qiao, Qingcai Jiang, Jun Shi, Junshi Chen, and Hong An. 
> CISim: ISA-Agnostic Custom Instruction Simulation for General-Purpose Processor. In 2026 Design, Automation and Test in Europe Conference (DATE ’26)

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

A example.txt will be generated. To run the simulator:

```bash
/path/to/simulator/sim/build/CISim aes_run_rn.bc example.txt /path/to/simulator/sim/ino.yml None None None None 0 0
```


