rm *.ll
rm *.yaml
#./build/adaptor   \
#/staff/haoxiaoyu/uarch_exp/simulator/auto/results/aes/aes/novia/aes_run_rn.bc  \
#/staff/haoxiaoyu/uarch_exp/simulator/auto/results/aes/aes/novia/to_debug_pass \
# /staff/haoxiaoyu/uarch_exp/simulator/auto/results/aes/aes/novia/cp_ops_data \
#  /staff/haoxiaoyu/uarch_exp/simulator/auto/results/aes/aes/novia/fu_ops_data  \
#  3 \
#   /staff/haoxiaoyu/uarch_exp/simulator/auto/results/aes/aes/novia/bblist.txt  \
#   output  \
#   3
#./build/adaptor   \
./build/dyser_offload   \
/staff/haoxiaoyu/uarch_exp/simulator/spec_dyser_ooo2/results/458.sjeng/novia/458.sjeng_run_rn.bc  \
/staff/haoxiaoyu/uarch_exp/simulator/spec_dyser_ooo2/results/458.sjeng/novia/to_debug_pass \
/staff/haoxiaoyu/uarch_exp/simulator/spec_dyser_ooo2/results/458.sjeng/novia/cp_ops_data \
/staff/haoxiaoyu/uarch_exp/simulator/spec_dyser_ooo2/results/458.sjeng/novia/fu_ops_data  \
  3 \
/staff/haoxiaoyu/uarch_exp/simulator/spec_dyser_ooo2/results/458.sjeng/novia/bblist.txt  \
   output  \
   3
#./build/scheduler  output.ll output 
