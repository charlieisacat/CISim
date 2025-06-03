#ifndef _PREDICTOR_H_
#define _PREDICTOR_H_

#include <bitset>
#include "../utils.h"

#define NUMTAGTABLES 4

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////
// Each entry in the tag Pred Table
struct TagEntry
{
    int32_t ctr;
    uint32_t tag;
    int32_t usefulBits;
};

// Folded History implementation ... from ghr (geometric length) -> compressed(target)
struct CompressedHist
{
    /// Objective is to convert geomLengt of GHR into tagPredLog which is of length equal to the index of the corresponding bank
    // It can be also used for tag when the final length would be the Tag
    uint32_t geomLength;
    uint32_t targetLength;
    uint32_t compHist;

    void updateCompHist(std::bitset<131> ghr)
    {

        /*bitset<131> temp;;
        int numFolds = geomLength / targetLength;
        //int remainder = (ghr.size()) % compLength;
        int mask = (1 << targetLength) - 1;
        int geometricMask = (1 << geomLength) - 1;
        temp = ghr;

        temp = temp & geometricMask;
        uint32_t remainder = 0;
        uint32_t comphistfolds[numFolds];

        for(int i=0; i < numFolds; i++)
        {
            comphistfolds[i] = temp & mask;
            temp = temp >> targetLength;

        }
        remainder = temp & mask;

        for(int i = 0; i< numFolds; i++)
        {
            compHist ^= comphistfolds[i];
        }
        compHist ^= remainder;*/
        // easier way below... see PPM paper Figure 2
        // creating important masks
        int mask = (1 << targetLength) - 1;
        int mask1 = ghr[geomLength] << (geomLength % targetLength);
        int mask2 = (1 << targetLength);
        compHist = (compHist << 1) + ghr[0];
        compHist ^= ((compHist & mask2) >> targetLength);
        compHist ^= mask1;
        compHist &= mask;
    }
};
class PREDICTOR
{

    // The state is defined for Gshare, change for your design

private:
    std::bitset<131> GHR; // global history register
    // 16 bit path history
    int PHR;
    // Bimodal
    uint32_t *bimodal;          // pattern history table
    uint32_t historyLength;     // history length
    uint32_t numBimodalEntries; // entries in pht
    uint32_t bimodalLog;
    // Tagged Predictors
    TagEntry *tagPred[NUMTAGTABLES];
    uint32_t numTagPredEntries;
    uint32_t tagPredLog;
    uint32_t geometric[NUMTAGTABLES];
    // Compressed Buffers
    CompressedHist indexComp[NUMTAGTABLES];
    CompressedHist tagComp[2][NUMTAGTABLES];

    // Predictions need to be global ?? recheck
    bool primePred;
    bool altPred;
    int primeBank;
    int altBank;
    // index had to be made global else recalculate for update
    uint32_t indexTagPred[NUMTAGTABLES];
    uint32_t tag[NUMTAGTABLES];
    uint32_t clock;
    int clock_flip;
    int32_t altBetterCount;

    bool TAKEN = 1;
    bool NOT_TAKEN = 0;

public:
    // The interface to the four functions below CAN NOT be changed

    PREDICTOR();
    bool GetPrediction(uint32_t PC);
    void UpdatePredictor(uint32_t PC, bool resolveDir, bool predDir, uint32_t branchTarget);

#if 0
  void    TrackOtherInst(uint32_t PC, OpType opType, uint32_t branchTarget);
#endif

    // Contestants can define their own functions below
};

/***********************************************************/
#endif
