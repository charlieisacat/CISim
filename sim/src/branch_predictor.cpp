#include "branch_predictor.h"
#include "tage/tage.h"

#include <iostream>

Bpred::Bpred(TypeBpred type, int bht_size, int ghrSize) : type(type), bht_size(bht_size)
{

  // 1-bit saturating counters BP
  if (type == bp_onebit)
  {
    bht = new int[bht_size];
    for (int i = 0; i < bht_size; i++)
      bht[i] = 1; // init to taken
  }
  // 2-bit saturating counters BP
  else if (type == bp_twobit)
  {
    bht = new int[bht_size];
    for (int i = 0; i < bht_size; i++)
      bht[i] = 2; // init to weakly taken
  }
  // gshare BP: made of a table of 2-bit saturating counters + a global history register
  else if (type == bp_gshare)
  {
    gshare_global_hist = 0;
    gshare_global_hist_bits = ghrSize;
    bht = new int[bht_size];
    for (int i = 0; i < bht_size; i++)
      bht[i] = 1; // init to weakly not-taken
  }
  else if (type == bp_tage)
  {
    // Initialize the TAGE predictor
    tagePredictor = new PREDICTOR();
  }
  else if (type != bp_none && type != bp_always_NT && type != bp_always_T && type != bp_perfect)
  {
    std::cout << "Unknown branch predictor!!!\n";
    assert(false);
  }
}

// predict a branch and return if the prediction was correct
bool Bpred::predict(uint64_t pc, bool actual_taken)
{
  if (type == bp_none)
  {
    return false;
  }
  else if (type == bp_perfect)
  {
    return true;
  }
  // models the behavior of an Always-NOT-Taken static branch predictor
  else if (type == bp_always_NT)
  {
    if (actual_taken)
      return false;
    else
      return true;
  }
  // models the behavior of an Always-Taken static branch predictor
  else if (type == bp_always_T)
  {
    if (actual_taken)
      return true;
    else
      return false;
  }
  // dynamic branch predictor with 1-bit saturating counters
  else if (type == bp_onebit)
  {
    // query the predictor
    int idx = pc & (bht_size - 1);
    int pred_taken = (bht[idx] == 1);

    // update the 1-bit counter
    if (actual_taken)
    {
      bht[idx]++;                       // increase counter
      bht[idx] = std::min(bht[idx], 1); // force the counter to be in the range [0..1]
    }
    else
    {
      bht[idx]--;                       // decrease counter
      bht[idx] = std::max(bht[idx], 0); // force the counter to be in the range [0..1]
    }
    assert(bht[idx] >= 0 && bht[idx] <= 1);
    return (pred_taken == actual_taken);
  }
  // dynamic branch predictor with 2-bit saturating counters
  else if (type == bp_twobit)
  {
    // query the predictor
    int idx = pc & (bht_size - 1);
    int pred_taken = (bht[idx] >= 2); // 0,1 not taken  |  2,3 taken

    // update the 2-bit counter
    if (actual_taken)
    {
      bht[idx]++;                       // increase counter
      bht[idx] = std::min(bht[idx], 3); // force the counter to be in the range [0..3]
    }
    else
    {
      bht[idx]--;                       // decrease counter
      bht[idx] = std::max(bht[idx], 0); // force the counter to be in the range [0..3]
    }
    assert(bht[idx] >= 0 && bht[idx] <= 3);
    return (pred_taken == actual_taken);
  }
  // GSHARE branch predictor with 2-bit saturating counters
  else if (type == bp_gshare)
  {
    // query the predictor
    int idx = (pc ^ gshare_global_hist) & (bht_size - 1);
    int pred_taken = (bht[idx] >= 2); // 0,1 not taken  |  2,3 taken

    // update the 2-bit counter
    if (actual_taken)
    {
      bht[idx]++;                       // increase counter
      bht[idx] = std::min(bht[idx], 3); // force the counter to be in the range [0..3]
    }
    else
    {
      bht[idx]--;                       // decrease counter
      bht[idx] = std::max(bht[idx], 0); // force the counter to be in the range [0..3]
    }
    assert(bht[idx] >= 0 && bht[idx] <= 3);

    // update the global history register
    int mask = (1 << gshare_global_hist_bits) - 1;
    gshare_global_hist = ((gshare_global_hist << 1) | actual_taken) & mask;
    return (pred_taken == actual_taken);
  }
  else if(type == bp_tage)
  {
    // query the TAGE predictor
    bool pred_taken = tagePredictor->GetPrediction(pc);
    
    // update the TAGE predictor
    tagePredictor->UpdatePredictor(pc, actual_taken, pred_taken, 0);
    
    return (pred_taken == actual_taken);
  }
  else
    return true; // default to perfect branch predictor
}