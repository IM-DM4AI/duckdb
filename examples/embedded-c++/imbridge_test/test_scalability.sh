#! /bin/bash

root_path="/root/workspace/duckdb/examples/embedded-c++/imbridge_test"
python_exe="/root/miniconda3/envs/tpc_ai/bin/python"

imlane_sh="$root_path/test_end2end.sh"
file_raven_path="$root_path/raven"
file_tpcx_ai_path="$root_path/tpcx_ai"
core=$3
dataset=$1
workload=$2

# echo "core_num: $core dataset: $dataset workload: $workload"
# docker exec -it IMLane_core_$core $imlane_sh $dataset $workload

for i in {1..16}
do

echo "core_num: $i dataset: $dataset workload: $workload"
docker exec -it IMLane_core_$i $imlane_sh $dataset $workload
done

