# This is the simulator we used for in our work CIExplorer: Microarchitecture-Aware Exploration for Tightly Integrated Custom InstructionCIExplorer (ICS'25)

## Citation
> Xiaoyu Hao, Sen Zhang, Liang Qiao, Qingcai Jiang, Jun Shi, Junshi Chen, Hong An, Xulong Tang, Hao Shu, and Honghui Yuan. 
> CIExplorer: Microarchitecture-Aware Exploration for Tightly Integrated Custom Instruction. In 2025 International Conference on Supercomputing (ICS ’25)

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


