#include <iostream>
#include <vector>
#include <string>
#include <map>

#include <iostream>
#include "task/instruction.h"
#include "task/basic_block.h"
#include "task/function.h"
#include "task/task.h"
#include "core.h"
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include "params.h"
#include <chrono>
#include "function_unit.h"
#include <regex>
#include "branch_predictor.h"

#include "accel.h"

using namespace std;

vector<int> parseFUs(string fus)
{
    regex fuPattern("\\+");

    sregex_token_iterator iter(fus.begin(), fus.end(), fuPattern, -1);
    sregex_token_iterator end;

    vector<int> tokens;
    while (iter != end)
    {
        assert(strFUMap.find(*iter) != strFUMap.end());
        tokens.push_back(strFUMap.find(*iter)->second);
        iter++;
    }

    return tokens;
}

vector<int> parsePortsCustom(const string ports_str, int offset = 0)
{
    vector<int> result;
    result.push_back(stoi(ports_str) + offset);
    return result;
}

vector<int> parsePorts(const string ports_str, int offset = 0)
{
    vector<int> result;
    for (char ch : ports_str)
    {
        if (isdigit(ch))
        {
            result.push_back(ch - '0' + offset);
        }
        else
        {
            cerr << "Error: Non-digit character encountered in ports: " << ch << endl;
        }
    }
    return result;
}

void loadCustomInsn(const string &filename, shared_ptr<Params> params)
{
    YAML::Node config = YAML::LoadFile(filename);

    const auto &cstmNode = config["Custom"];

    // parse ports
    int numPort = params->portConfigs.size();
    int portID = numPort;
    for (const auto &port : config["ports"])
    {
        int id = port.second["id"].as<int>();
        int fu = id + (int)FUType::COUNT;
        int fu_n = 1;

        if (id == 0)
        {
            params->portConfigs[3].push_back(fu);
            params->portConfigs[4].push_back(fu);
        }
        if (id == 1)
        {
            params->portConfigs[0].push_back(fu);
            params->portConfigs[1].push_back(fu);
        }
        // else
        // {
        //     for (int i = 0; i < fu_n; i++)
        //         params->portConfigs[portID].push_back(fu);
        //     params->notPipelineFUs.push_back(fu);
        //     portID++;
        // }
    }
    int offset = numPort;
    // parse instructions
    for (const auto &node : config["instructions"])
    {
        shared_ptr<InstConfig> inst = make_shared<InstConfig>();
        string portMapping = node.second["ports"].as<string>();

        int id = node.second["fu"].as<int>();
        string nodeName = node.first.as<string>();
        vector<int> ports;

        vector<int> fus;
        int fu = id + (int)FUType::COUNT;
        fus.push_back(fu);

        if (nodeName == "packing")
        {
            // TODO: config
            ports = {3, 4};
            inst->ports.push_back({1, ports});
        }
        else if (nodeName == "extract")
        {
            ports = {0, 1};
            inst->ports.push_back({1, ports});
        }
        else
        {
            // cout<<"portMapping="<<portMapping<<" offset="<<offset<<"\n";
            ports = parsePortsCustom(portMapping, offset - 2);
            inst->ports.push_back({1, ports});

            for (auto portID : ports)
            {
                params->portConfigs[portID].push_back(fu);
                // cout<<"portID="<<portID<<" fu="<<fu<<"\n";
                params->notPipelineFUs.push_back(fu);
            }
        }

        inst->opcode_num = node.second["opcode_num"].as<int>();
        inst->runtime_cycles = node.second["runtime_cycles"].as<int>();
        inst->fus = {fus};
        params->insnConfigs[inst->opcode_num] = inst;
    }
}

void loadConfig(const string &filename, shared_ptr<Params> params)
{
    YAML::Node config = YAML::LoadFile(filename);

    const auto &cpuNode = config["CPU"];
    params->windowSize = cpuNode["window_size"].as<uint64_t>();
    params->retireSize = cpuNode["retire_size"].as<uint64_t>();
    params->flushPenalty = cpuNode["flush_penalty"].as<uint64_t>();
    params->robSize = cpuNode["rob_size"].as<uint64_t>();
    params->clockSpeed = cpuNode["clock_speed"].as<int>();
    params->issueQSize = cpuNode["issueq_size"].as<int>();
    params->naive = cpuNode["naive"].as<bool>();
    params->ldqSize = cpuNode["ldq_size"].as<uint64_t>();
    params->stqSize = cpuNode["stq_size"].as<int>();
    params->lsu_port = cpuNode["lsu_port"].as<int>();

    const auto &dramNode = config["DRAM"];
    params->simpleDram = dramNode["simple"].as<bool>();
    params->dram_latency = dramNode["latency"].as<int>();
    params->dram_bw = dramNode["bw"].as<int>();
    params->DRAM_system = dramNode["system"].as<string>();
    params->DRAM_device = dramNode["device"].as<string>();

    const auto &l1Node = config["L1Cache"];
    params->l1_latency = l1Node["latency"].as<int>();
    params->l1_CLSize = l1Node["cl_size"].as<int>();
    params->l1_assoc = l1Node["assoc"].as<int>();
    params->l1_size = l1Node["size"].as<int>();
    params->l1_mshr = l1Node["mshr_size"].as<int>();
    params->l1_store_ports = l1Node["store_ports"].as<int>();
    params->l1_load_ports = l1Node["load_ports"].as<int>();

    const auto &l2Node = config["L2Cache"];
    params->l2_latency = l2Node["latency"].as<int>();
    params->l2_CLSize = l2Node["cl_size"].as<int>();
    params->l2_assoc = l2Node["assoc"].as<int>();
    params->l2_size = l2Node["size"].as<int>();
    params->l2_mshr = l2Node["mshr_size"].as<int>();
    params->l2_store_ports = l2Node["store_ports"].as<int>();
    params->l2_load_ports = l2Node["load_ports"].as<int>();

    const auto &l3Node = config["L3Cache"];
    params->l3_latency = l3Node["latency"].as<int>();
    params->l3_CLSize = l3Node["cl_size"].as<int>();
    params->l3_assoc = l3Node["assoc"].as<int>();
    params->l3_size = l3Node["size"].as<int>();
    params->l3_mshr = l3Node["mshr_size"].as<int>();
    params->l3_store_ports = l3Node["store_ports"].as<int>();
    params->l3_load_ports = l3Node["load_ports"].as<int>();

    const auto &bpNode = config["BPred"];
    params->bhtSize = bpNode["bhtSize"].as<int>();
    string bpTypeStr = bpNode["type"].as<string>();
    params->ghrSize = bpNode["ghrSize"].as<int>();
    params->bpType = strBPMap.find(bpTypeStr)->second;

    if (config["not_pipeline_fu"] && config["not_pipeline_fu"].IsSequence())
    {
        vector<string> names = config["not_pipeline_fu"].as<vector<string>>();

        // Print the names
        for (const auto &name : names)
        {
            // cout<<" not pipeline: "<<strFUMap.find(name)->second<<"\n";
            params->notPipelineFUs.push_back(strFUMap.find(name)->second);
        }
    }

    // parse instructions
    for (const auto &node : config["instructions"])
    {
        shared_ptr<InstConfig> inst = make_shared<InstConfig>();

        // parse port mapping
        string portMapping = node.second["ports"].as<string>();
        // cout << "portMapping=" << portMapping << "\n";

        regex pattern(R"((\d+)\*(\d+))"); // Pattern to match "multiplier*value"
        smatch matches;
        // a flag to indicate if ports in uops.info format
        bool flag = false;

        while (regex_search(portMapping, matches, pattern))
        {
            flag = true;
            int num = stoi(matches[1].str());
            auto ports = parsePorts(matches[2].str());
            inst->ports.push_back({num, ports});

            portMapping = matches.suffix().str();
        }

        if (!flag)
        {
            auto ports = parsePorts(node.second["ports"].as<string>());
            inst->ports.push_back({1, ports});
        }

        vector<vector<int>> tokens;

        // parse fu
        if (node.second["fu"].IsSequence())
        {
            // Iterate over the sequence and push into the vector
            for (const auto &fuStr : node.second["fu"])
            {
                auto fus = parseFUs(fuStr.as<string>());
                tokens.push_back(fus);
            }
        }
        else
        {
            vector<int> fus = parseFUs(node.second["fu"].as<string>());
            tokens.push_back(fus);
        }

        inst->opcode_num = node.second["opcode_num"].as<int>();
        inst->runtime_cycles = node.second["runtime_cycles"].as<int>();
        inst->fus = tokens;
        params->insnConfigs[inst->opcode_num] = inst;
    }

    // parse ports
    int portID = 0;
    for (const auto &port : config["ports"])
    {
        vector<int> fuTypes;
        for (const auto &fu : port.second)
        {
            fuTypes.push_back(strFUMap.find(fu.second.as<string>())->second);
        }
        params->portConfigs[portID] = fuTypes;
        portID++;
    }
}

void alias_analysis(llvm::Module &m)
{
    llvm::PassBuilder passBuilder;
    llvm::ModuleAnalysisManager MAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::LoopAnalysisManager LAM;
    llvm::CGSCCAnalysisManager CGAM;

    // Registering the necessary analysis managers
    passBuilder.registerModuleAnalyses(MAM);
    passBuilder.registerCGSCCAnalyses(CGAM);
    passBuilder.registerFunctionAnalyses(FAM);
    passBuilder.registerLoopAnalyses(LAM);

    // Cross-register analysis managers to enable inter-manager communication
    passBuilder.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    for (auto &Func : m)
    {
        if (Func.isDeclaration())
            continue; // Skip function declarations

        llvm::outs() << " funname=" << Func.getName().str() << "\n";

        // Retrieve Alias Analysis results for the function
        llvm::AAResults &AA = FAM.getResult<llvm::AAManager>(Func);

        // Iterate through instructions to find pairs of pointers to analyze
        for (auto &BB : Func)
        {
            vector<llvm::Instruction *> loadStoreInsts;

            // Collect all load and store instructions
            for (auto &Inst : BB)
            {
                if (llvm::isa<llvm::LoadInst>(&Inst) || llvm::isa<llvm::StoreInst>(&Inst))
                {
                    loadStoreInsts.push_back(&Inst);
                }
            }

            // Check aliasing between collected load and store instructions
            for (auto *Inst1 : loadStoreInsts)
            {
                for (auto *Inst2 : loadStoreInsts)
                {
                    if (Inst1 == Inst2)
                        continue; // Skip self comparisons

                    // Perform the alias check
                    if (AA.alias(Inst1, Inst2) == llvm::AliasResult::NoAlias)
                    {
                        llvm::outs() << "No alias between: " << *Inst1 << " and " << *Inst2 << "\n";
                    }
                    else
                    {
                        llvm::outs() << "Potential alias between: " << *Inst1 << " and " << *Inst2 << "\n";
                    }
                }
            }
        }
    }
}

set<string> readFuncNames(string filename)
{
    std::fstream file;
    set<string> ret;

    file.open(filename);

    std::string funcName;
    while (file >> funcName)
    {
        ret.insert(funcName);
    }

    return ret;
}

std::unordered_map<uint64_t, std::vector<uint64_t>> readInitFile(string filename)
{
    std::unordered_map<uint64_t, std::vector<uint64_t>> uid_deps_map;

    // Open the input file
    std::ifstream infile(filename);
    std::string line;

    // Read each line from the file
    while (std::getline(infile, line))
    {
        // Create a stringstream to parse the line
        std::stringstream ss(line);
        std::string key_str, values_str;

        // Extract key and values part
        if (std::getline(ss, key_str, ':') && std::getline(ss, values_str))
        {
            uint64_t key = std::stoull(key_str); // Convert the key to uint64_t

            // Create a vector to store the dependencies
            std::vector<uint64_t> dependencies;

            // Parse the values (dependencies)
            std::stringstream values_ss(values_str);
            std::string value_str;
            while (std::getline(values_ss, value_str, ','))
            {
                dependencies.push_back(std::stoull(value_str)); // Convert each value to uint64_t and store in vector
            }

            // Insert the key and its dependencies into the map
            uid_deps_map[key] = dependencies;
        }
    }

#if 0
    // Optional: Print the map for verification
    for (const auto &pair : uid_deps_map)
    {
        std::cout << "Key: " << pair.first << " -> Values: ";
        for (const auto &dep : pair.second)
        {
            std::cout << dep << " ";
        }
        std::cout << std::endl;
    }
#endif

    return uid_deps_map;
}

#ifdef _OPENMP
#include <omp.h>
// getting maximum number of threads available
static const int MAX_THREADS = (omp_get_thread_num() == 0) ? omp_get_max_threads() : omp_get_thread_num();
#endif

int main(int argc, char **argv)
{
    auto start = chrono::high_resolution_clock::now();
    string ir_file = string(argv[1]);
    string trace_file = string(argv[2]);
    string config_file = string(argv[3]);
    string start_bb_name = string(argv[4]);
    string funcname_file = string(argv[5]);
    string init_file_name = string(argv[6]);
    string offload_func_name = string(argv[7]);
    int acc_speculative = stoi(string(argv[8]));
    int dyser = stoi(string(argv[9]));

    set<string> funcNames;
    if (funcname_file != "None")
        funcNames = readFuncNames(funcname_file);

    if (funcNames.size() && offload_func_name != "None")
    {
        funcNames.insert(offload_func_name);
    }

    bool funcUseWhiteList = funcNames.size() > 0;

    cout << "funcUseWhiteList=" << funcUseWhiteList << " size=" << funcNames.size() << "\n";

    unordered_map<uint64_t, vector<uint64_t>> uid_deps_map;
    bool from_init_file = false;
    if (init_file_name != "None")
    {
        from_init_file = true;
        uid_deps_map = readInitFile(init_file_name);
    }

    llvm::StringRef file = ir_file;
    unique_ptr<llvm::LLVMContext> context(new llvm::LLVMContext());
    unique_ptr<llvm::SMDiagnostic> error(new llvm::SMDiagnostic());
    unique_ptr<llvm::Module> m;

    m = llvm::parseIRFile(file, *error, *context);
    unordered_map<llvm::Value *, shared_ptr<SIM::Value>> value_map;
    vector<shared_ptr<SIM::Function>> func_list;
    shared_ptr<SIM::Function> main_func;
    shared_ptr<SIM::Function> dyser_func = nullptr;

    unordered_map<std::string, llvm::Value *> name_bb_map;

    //    alias_analysis(*m);

    shared_ptr<Params> params = make_shared<Params>();
    map<string, shared_ptr<InstConfig>> configs;
    loadConfig(config_file, params);

    params->offload_func_name = offload_func_name;
    params->acc_speculative = acc_speculative > 0;
    params->dyser = dyser > 0;

    unordered_map<uint64_t, llvm::Instruction *> uid_insn_map;
    unordered_map<int, shared_ptr<SIM::Function>> dyser_func_map;

    int funcNum = 0;
    int bbNum = 0;
    uint64_t uid = 0;
    for (auto &func : *m)
    {
        string funcName = func.getName().str();
        bool dyser_enap = funcName.find("encapsulated_function") != std::string::npos;
        // decl and intrin func will never be fetched ?
        if (funcUseWhiteList && !funcNames.count(funcName) && !dyser_enap)
            continue;

        shared_ptr<SIM::Function> sfunc = make_shared<SIM::Function>(uid++, &func);
        value_map.insert(make_pair<llvm::Value *, shared_ptr<SIM::Value>>(&func, sfunc));
        func_list.push_back(sfunc);

        if (funcName == "main")
            main_func = sfunc;

        if (dyser && funcName.find("encapsulated_function") != std::string::npos)
        {
            if (llvm::MDNode *CustomMD = sfunc->getLLVMFunction()->getMetadata(sfunc->getFuncName()))
            {
                int opcode = 0;
                for (const auto &op : CustomMD->operands())
                {
                    auto *meta = llvm::dyn_cast<llvm::MDString>(op.get());
                    opcode = std::stoi(meta->getString().str());
                    break;
                }
                // cout << "funcName=" << funcName << " opcode=" << opcode << "\n";
                dyser_func_map.insert({opcode, sfunc});
            }
        }

        for (auto &bb : func)
        {
            shared_ptr<SIM::BasicBlock> sbb = make_shared<SIM::BasicBlock>(uid++, &bb);
            value_map.insert(make_pair<llvm::Value *, shared_ptr<SIM::Value>>(&bb, sbb));
            if (from_init_file)
                sfunc->addBB(sbb);

            for (auto &insn : bb)
            {
                shared_ptr<SIM::Instruction> s_insn = make_shared<SIM::Instruction>(uid++, &insn);
                value_map.insert(make_pair<llvm::Value *, shared_ptr<SIM::Value>>(&insn, s_insn));
                if (from_init_file)
                    sbb->addInsn(s_insn);
                if (!params->insnConfigs.count(s_insn->getOpCode()))
                {
                    cout << "name=" << s_insn->getOpName() << " " << s_insn->getOpCode() << "\n";
                }
                assert(params->insnConfigs.count(s_insn->getOpCode()));

                uid_insn_map[s_insn->getUID()] = &insn;
            }

            name_bb_map[sbb->getBBName()] = &bb;
            bbNum++;
        }

        funcNum++;
        if (funcNum % 100 == 0)
        {
            cout << "read " << funcNum << " functions" << "\n";
        }
    }

    cout << "total func=" << funcNum << " bb=" << bbNum << " vmap size=" << value_map.size() << " uid mapsize=" << uid_insn_map.size() << "\n";

    vector<vector<string>> func_data(func_list.size(), vector<string>());

    // for(auto item = dyser_func_map.begin(); item != dyser_func_map.end();item++){
    //     cout<<"opcode="<<item->first<<" func="<<item->second->getFuncName()<<"\n";
    // }

    if (!from_init_file)
    {
#ifdef _OPENMP
#pragma omp parallel for num_threads(MAX_THREADS)
#endif
        for (int i = 0; i < func_list.size(); i++)
        {
            auto sfunc = func_list[i];
            sfunc->initialize(value_map);
            // auto data = sfunc->getInsnDepData();
            // func_data[i] = data;

            // cout << "finish func=" << sfunc->getFuncName() << " insn size=" << data.size() << "\n";
        }
    }
    else
    {
        cout << "init from " << init_file_name << "\n";
#ifdef _OPENMP
#pragma omp parallel for num_threads(MAX_THREADS)
#endif
        for (int i = 0; i < func_list.size(); i++)
        {
            auto sfunc = func_list[i];
            for (auto sbb : sfunc->getBBList())
            {
                for (auto si : sbb->getInstructions())
                {
                    uint64_t uid = si->getUID();
                    if (uid_deps_map.count(uid))
                    {
                        auto deps = uid_deps_map[uid];
                        for (auto dep : deps)
                        {
                            si->addStaticDependency(value_map[uid_insn_map[dep]]);
                        }
                    }
                }
            }
            auto data = sfunc->getInsnDepData();
            func_data[i] = data;
        }
    }


    shared_ptr<SIM::BasicBlock> entry = nullptr;
    bool simpoint = false;
    if (start_bb_name == "None")
    {
        entry = main_func->getEntryBB();
    }
    else
    {
        assert(name_bb_map.count(start_bb_name));
        llvm::Value *value = name_bb_map[start_bb_name];
        assert(value_map.count(value));
        entry = std::dynamic_pointer_cast<SIM::BasicBlock>(value_map[value]);
        cout << "entry =" << entry->getBBName() << "\n";
        simpoint = true;
    }

    shared_ptr<SIM::Task> task = make_shared<SIM::Task>(entry, value_map, trace_file);
    task->name_bb_map = name_bb_map;
    task->initialize();

    double percentage = 1.0;
    if (argc >= 12)
    {
        string custom_config_file = string(argv[10]);
        percentage = stod(string(argv[11]));
        loadCustomInsn(custom_config_file, params);
    }

    cout<<"after load\n";

#if 0
    auto l1c = std::make_shared<L1Cache>(params->l1_latency, params->l1_CLSize, params->l1_size, params->l1_assoc, params->l1_mshr, params->l1_store_ports, params->l1_load_ports);
    auto l2c = std::make_shared<L2Cache>(params->l2_latency, params->l2_CLSize, params->l2_size, params->l2_assoc, params->l2_mshr, params->l2_store_ports, params->l2_load_ports);
    auto llc = make_shared<LLCache>(params->l3_latency, params->l3_CLSize, params->l3_size, params->l3_assoc, params->l3_mshr, params->l3_store_ports, params->l3_load_ports);
    auto memInterface = new DRAMSimInterface(llc, params->simpleDram, true, 4, 4, params->DRAM_system, params->DRAM_device);
    auto simpleDRAM = new SimpleDRAM(memInterface, params->dram_latency, params->dram_bw);

    l2c->nextCache = llc;
    memInterface->simpleDRAM = simpleDRAM;
    llc->prevCache = l2c;
    llc->memInterface = memInterface;
    memInterface->initialize(params->clockSpeed, llc->getCacheLineSize());

    auto lsu = std::make_shared<LSUnit>(l1c);

    // lsu and l1cache
    l1c->lsu = lsu;

    l1c->nextCache = l2c;
    l2c->prevCache = l1c;

    shared_ptr<Accelerator> acc = make_shared<Accelerator>(task, params);
    acc->lsu = lsu;
    acc->setEntry(entry);

    auto run_start = chrono::high_resolution_clock::now();
    while (acc->isRunning())
    {
        memInterface->process();
        llc->process();
        l2c->process();
        l1c->process();

        lsu->wb();
        lsu->process();

        acc->tick();
        if (acc->getCycle() % 100000 == 0)
        {
            auto run_stop = chrono::high_resolution_clock::now();
            auto run_duration = chrono::duration_cast<chrono::milliseconds>(run_stop - run_start);
            cout << "Simulating at cycle: " << acc->getCycle()
                 << ", num of simulated insns: " << acc->totalCommit
                 << ", wall-time: " << run_duration.count() * 1.0 / 1000 << " sec \n";
        }

        if (acc->isFinish())
        {
            acc->stop();
        }
    }
    acc->printStats();
#endif

#if 1
    shared_ptr<Core> core = make_shared<Core>(task, params);
    core->percentage = percentage;

    // this should be set before core init
    if (dyser_func_map.size())
        core->setDyserEntry(dyser_func_map);

    core->initialize();
    core->setSimpoint(simpoint);
    core->getAccelerator()->setSimpoint(simpoint);

    auto llc = make_shared<LLCache>(params->l3_latency, params->l3_CLSize, params->l3_size, params->l3_assoc, params->l3_mshr, params->l3_store_ports, params->l3_load_ports);
    auto memInterface = new DRAMSimInterface(llc, params->simpleDram, true, 4, 4, params->DRAM_system, params->DRAM_device);
    auto simpleDRAM = new SimpleDRAM(memInterface, params->dram_latency, params->dram_bw);

    vector<shared_ptr<Hardware>> tiles;

    core->getL2Cache()->nextCache = llc;
    memInterface->simpleDRAM = simpleDRAM;
    llc->prevCache = core->getL2Cache();
    llc->memInterface = memInterface;
    memInterface->initialize(params->clockSpeed, llc->getCacheLineSize());

    tiles.push_back(core);
    auto run_start = chrono::high_resolution_clock::now();
    while (core->isRunning())
    {
        memInterface->process();
        llc->process();
        tiles[0]->run();
        if (core->getCycle() % 1000000 == 0)
        {
            auto run_stop = chrono::high_resolution_clock::now();
            auto run_duration = chrono::duration_cast<chrono::milliseconds>(run_stop - run_start);
            cout << "Simulating at cycle: " << core->getCycle()
                 << ", num of simulated insns: " << core->getTotalCommit()
                 << ", wall-time: " << run_duration.count() * 1.0 / 1000 << " sec \n";
        }
    }
    task->finish();
    core->finish();
    core->printStats();
    core->printForMcpat();
#endif

    // End time
    auto stop = chrono::high_resolution_clock::now();
    // Calculate duration
    auto duration = chrono::duration_cast<chrono::milliseconds>(stop - start);

    cout << "Simulation Time taken: " << duration.count() * 1.0 / 1000 << " seconds" << endl;
    return 1;
}
