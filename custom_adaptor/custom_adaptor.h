#ifndef INCLUDE_CUSTOM_ADAPTOR_H
#define INCLUDE_CUSTOM_ADAPTOR_H

#include <string>
#include <vector>
#include <fstream>
#include <map>
#include <cassert>

struct CandidateComponent
{
    uint64_t cid = 0;

    std::string bbName = "";

    // global uid
    std::vector<uint64_t> vertices;
    // from uid -> to uid
    std::vector<std::pair<uint64_t, uint64_t>> edges;

    std::string fuStr = "";
    std::string cpStr = "";

    int opcode = -1;
    double score = 0.;

    CandidateComponent(uint64_t _cid) : cid(_cid) {}
};

typedef std::vector<CandidateComponent *> Candidate;

std::vector<std::vector<uint64_t>> readCriticalPathOps(std::string filename)
{
    std::vector<std::vector<uint64_t>> ops;
    std::fstream f;
    f.open(filename);
    std::string s;

    while (!(f >> s).eof())
    {
        if (s == "t")
        {
            ops.push_back(std::vector<uint64_t>());
        }
        else
        {
            ops.back().push_back(std::stoull(s));
        }
    }

    f.close();
    return ops;
}

Candidate readCustomInsn(std::string filename)
{
    Candidate candidate;
    std::fstream f;
    f.open(filename);

    std::string label, _;
    uint64_t cid;     // component id
    uint64_t frm, to; // vid
    uint64_t uid;
    int64_t opcode;
    uint64_t vid;
    std::string bbName;
    double score = 0.;

    std::map<uint64_t, uint64_t> vidUidMap;

    while (!(f >> label).eof())
    {
        if (label == "t")
        {
            f >> _ >> cid;
            candidate.push_back(new CandidateComponent(cid));
            vidUidMap.clear();
        }
        else if (label == "e")
        {
            f >> frm >> to >> _;
            if (vidUidMap.find(frm) != vidUidMap.end() && vidUidMap.find(to) != vidUidMap.end())
            {
                candidate.back()->edges.push_back(std::make_pair(vidUidMap[frm], vidUidMap[to]));
            }
        }
        else if (label == "v")
        {
            f >> vid >> opcode >> _ >> _ >> _ >> bbName >> _ >> uid;

            if (opcode > 0)
            {
                assert(vidUidMap.find(vid) == vidUidMap.end());
                vidUidMap[vid] = uid;
                candidate.back()->vertices.push_back(uid);
                candidate.back()->bbName = bbName;
            }
        }else if(label == "s")
        {
            f >> score; 
            candidate.back()->score = score;
        }
    }

    f.close();

    return candidate;
}


#endif