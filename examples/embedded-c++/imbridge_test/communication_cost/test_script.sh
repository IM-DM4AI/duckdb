#!/bin/bash

echo "_________________________PF1______________________________"

for i in {1..5}
do
    echo "local $i" 
    /root/miniconda3/envs/tpc_ai/bin/python /root/workspace/duckdb/examples/embedded-c++/imbridge_test/communication_cost/pf1_infer.py udf
done

for i in {1..5}
do
    echo "local $i" 
    /root/miniconda3/envs/tpc_ai/bin/python /root/workspace/duckdb/examples/embedded-c++/imbridge_test/communication_cost/pf1_infer.py origin
done

# for i in {1..5}
# do
#     echo "spark $i" 
#     /root/miniconda3/envs/tpc_ai/bin/python /root/workspace/duckdb/examples/embedded-c++/imbridge_test/communication_cost/pf1_sparksql.py udf
# done

# for i in {1..5}
# do
#     echo "spark $i" 
#     /root/miniconda3/envs/tpc_ai/bin/python /root/workspace/duckdb/examples/embedded-c++/imbridge_test/communication_cost/pf1_sparksql.py origin
# done

# for i in {1..5}
# do
#     echo "duckdb $i" 
#     /root/miniconda3/envs/tpc_ai/bin/python /root/workspace/duckdb/examples/embedded-c++/imbridge_test/communication_cost/pf1.py udf
# done

# for i in {1..5}
# do
#     echo "duckdb $i" 
#     /root/miniconda3/envs/tpc_ai/bin/python /root/workspace/duckdb/examples/embedded-c++/imbridge_test/communication_cost/pf1.py origin
# done


# for i in {1..5}
# do
#     echo "IMLane $i" 
#     /root/workspace/duckdb/examples/embedded-c++/build/imbridge_test/communication_cost/pf1_conmunication udf
# done

# for i in {1..5}
# do
#     echo "IMLane $i" 
#     /root/workspace/duckdb/examples/embedded-c++/build/imbridge_test/communication_cost/pf1_conmunication origin
# done

echo "_________________________PF7______________________________"


for i in {1..5}
do
    echo "local $i" 
    /root/miniconda3/envs/tpc_ai/bin/python /root/workspace/duckdb/examples/embedded-c++/imbridge_test/communication_cost/pf7_infer.py udf
done

for i in {1..5}
do
    echo "local $i" 
    /root/miniconda3/envs/tpc_ai/bin/python /root/workspace/duckdb/examples/embedded-c++/imbridge_test/communication_cost/pf7_infer.py origin
done


# for i in {1..5}
# do
#     echo "spark $i" 
#     /root/miniconda3/envs/tpc_ai/bin/python /root/workspace/duckdb/examples/embedded-c++/imbridge_test/communication_cost/pf7_sparksql.py udf
# done

# for i in {1..5}
# do
#     echo "spark $i" 
#     /root/miniconda3/envs/tpc_ai/bin/python /root/workspace/duckdb/examples/embedded-c++/imbridge_test/communication_cost/pf7_sparksql.py origin
# done

# for i in {1..5}
# do
#     echo "duckdb $i" 
#     /root/miniconda3/envs/tpc_ai/bin/python /root/workspace/duckdb/examples/embedded-c++/imbridge_test/communication_cost/pf7.py udf
# done

# for i in {1..5}
# do
#     echo "duckdb $i" 
#     /root/miniconda3/envs/tpc_ai/bin/python /root/workspace/duckdb/examples/embedded-c++/imbridge_test/communication_cost/pf7.py origin
# done


# for i in {1..5}
# do
#     echo "IMLane $i" 
#     /root/workspace/duckdb/examples/embedded-c++/build/imbridge_test/communication_cost/pf7_conmunication udf
# done

# for i in {1..5}
# do
#     echo "IMLane $i" 
#     /root/workspace/duckdb/examples/embedded-c++/build/imbridge_test/communication_cost/pf7_conmunication origin
# done

echo "_________________________UC10______________________________"

for i in {1..5}
do
    echo "local $i" 
    /root/miniconda3/envs/tpc_ai/bin/python /root/workspace/duckdb/examples/embedded-c++/imbridge_test/communication_cost/uc10_infer.py udf
done

for i in {1..5}
do
    echo "local $i" 
    /root/miniconda3/envs/tpc_ai/bin/python /root/workspace/duckdb/examples/embedded-c++/imbridge_test/communication_cost/uc10_infer.py origin
done

# for i in {1..5}
# do
#     echo "spark $i" 
#     /root/miniconda3/envs/tpc_ai/bin/python /root/workspace/duckdb/examples/embedded-c++/imbridge_test/communication_cost/uc10_sparksql.py udf
# done

# for i in {1..5}
# do
#     echo "spark $i" 
#     /root/miniconda3/envs/tpc_ai/bin/python /root/workspace/duckdb/examples/embedded-c++/imbridge_test/communication_cost/uc10_sparksql.py origin
# done

# for i in {1..5}
# do
#     echo "duckdb $i" 
#     /root/miniconda3/envs/tpc_ai/bin/python /root/workspace/duckdb/examples/embedded-c++/imbridge_test/communication_cost/uc10.py udf
# done

# for i in {1..5}
# do
#     echo "duckdb $i" 
#     /root/miniconda3/envs/tpc_ai/bin/python /root/workspace/duckdb/examples/embedded-c++/imbridge_test/communication_cost/uc10.py origin
# done


# for i in {1..5}
# do
#     echo "IMLane $i" 
#     /root/workspace/duckdb/examples/embedded-c++/build/imbridge_test/communication_cost/uc10_conmunication udf
# done

# for i in {1..5}
# do
#     echo "IMLane $i" 
#     /root/workspace/duckdb/examples/embedded-c++/build/imbridge_test/communication_cost/uc10_conmunication origin
# done
