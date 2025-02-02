#! /bin/bash

root_path="/root/workspace/duckdb/examples/embedded-c++/imbridge_test"
python_exe="/root/miniconda3/envs/tpc_ai/bin/python"

base_sh="$root_path/test_end2end.sh"
file_raven_path="$root_path/raven"
file_tpcx_ai_path="$root_path/tpcx_ai"

# echo "____________________________DuckDB___________________________________"

# echo "pf1"
# $python_exe $file_raven_path/pf1.py
# echo "pf2"
# $python_exe $file_raven_path/pf2.py
# echo "pf3"
# $python_exe $file_raven_path/pf3.py
# echo "pf4"
# $python_exe $file_raven_path/pf4.py
# echo "pf5"
# $python_exe $file_raven_path/pf5.py
# echo "pf6"
# $python_exe $file_raven_path/pf6.py
# echo "pf7"
# $python_exe $file_raven_path/pf7.py
# echo "pf8"
# $python_exe $file_raven_path/pf8.py


# echo "uc01"
# $python_exe $file_tpcx_ai_path/uc01.py
# echo "uc03"
# $python_exe $file_tpcx_ai_path/uc03.py
# echo "uc04"
# $python_exe $file_tpcx_ai_path/uc04.py
# echo "uc06"
# $python_exe $file_tpcx_ai_path/uc06.py
# echo "uc07"
# $python_exe $file_tpcx_ai_path/uc07.py
# # echo "uc08"
# # $python_exe $file_tpcx_ai_path/uc08.py
# echo "uc10"
# $python_exe $file_tpcx_ai_path/uc10.py

# echo "____________________________Local___________________________________"

# echo "pf1"
# $python_exe $file_raven_path/pf1_infer.py
# echo "pf2"
# $python_exe $file_raven_path/pf2_infer.py
# echo "pf3"
# $python_exe $file_raven_path/pf3_infer.py
# echo "pf4"
# $python_exe $file_raven_path/pf4_infer.py
# echo "pf5"
# $python_exe $file_raven_path/pf5_infer.py
# echo "pf6"
# $python_exe $file_raven_path/pf6_infer.py
# echo "pf7"
# $python_exe $file_raven_path/pf7_infer.py
# echo "pf8"
# $python_exe $file_raven_path/pf8_infer.py


# echo "uc01"
# $python_exe $file_tpcx_ai_path/uc01_infer.py
# echo "uc03"
# $python_exe $file_tpcx_ai_path/uc03_infer.py
# echo "uc04"
# $python_exe $file_tpcx_ai_path/uc04_infer.py
# echo "uc06"
# $python_exe $file_tpcx_ai_path/uc06_infer.py
# echo "uc07"
# $python_exe $file_tpcx_ai_path/uc07_infer.py
# # echo "uc08"
# # $python_exe $file_tpcx_ai_path/uc08_infer.py
# echo "uc10"
# $python_exe $file_tpcx_ai_path/uc10_infer.py

echo "____________________________IMLane(scheduler ml = 1)___________________________________"

echo "pf1"
$base_sh pf 1
echo "pf2"
$base_sh pf 2
echo "pf3"
$base_sh pf 3
echo "pf4"
$base_sh pf 4
echo "pf5"
$base_sh pf 5
echo "pf6"
$base_sh pf 6
echo "pf7"
$base_sh pf 7
echo "pf8"
$base_sh pf 8


echo "uc01"
$base_sh uc 01
# echo "uc03"
# $base_sh uc 03
echo "uc04"
$base_sh uc 04
echo "uc07"
$base_sh uc 07
# echo "uc08"
# $base_sh uc 08
echo "uc10"
$base_sh uc 10
echo "uc06"
$base_sh uc 06