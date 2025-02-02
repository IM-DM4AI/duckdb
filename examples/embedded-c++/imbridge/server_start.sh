#!/bin/bash
eval "$(conda shell.bash hook)"
conda activate tpc_ai
/root/workspace/duckdb/examples/embedded-c++/build/imbridge/udf_server $1  
# &
# PROGRAM_PID=$!
# perf=$(find /usr/lib/linux-tools/*/perf | head -1)

# $perf record  -p $PROGRAM_PID -o /root/workspace/duckdb/.vscode/perf_tmp/$1_perf.data
