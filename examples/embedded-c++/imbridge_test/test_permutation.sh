#! /bin/bash

root_path="/root/workspace/duckdb/examples/embedded-c++"
exe_path="$root_path/build/imbridge_test/permutation"
code_path="/root/workspace/duckdb/examples/embedded-c++/imbridge/code.py"
sys_core=$1


# #continue experiment
# c_now=5632
# rm -rf /dev/shm/*
# cat $root_path/imbridge_test/permutation/pf3_code_template.py > $code_path
# $exe_path/permutation_pf3 9 8 $c_now

# #batch work
# for tdb in $(seq 8 -1 1)
# do
#     rm -rf /dev/shm/*
#     cat $root_path/imbridge_test/permutation/pf6_code_template.py > $code_path
#     $exe_path/permutation_pf6 $tdb 8 $c_now
# done

# db work
# for ttml in {1..3}
# do
#     for ttdb in $(seq $sys_core -1 1)
#     do
#         rm -rf /dev/shm/*
#         cat $root_path/imbridge_test/permutation/pf3_code_template.py > $code_path
#         $exe_path/permutation_pf3 $ttdb $ttml
#     done
# done

# #formal experiment
workload=("pf4")
for job in "${workload[@]}"
do
    echo "workload : $job"
    for ml in $(seq 1 $sys_core)
    do
        for db in $(seq $sys_core -1 1)
        do
            rm -rf /dev/shm/*
            cat $root_path/imbridge_test/permutation/${job}_code_template.py > $code_path
            $exe_path/permutation_${job} $db $ml
        done
    done
done

