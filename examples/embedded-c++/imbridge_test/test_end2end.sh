#! /bin/bash

root_path="/root/workspace/duckdb/examples/embedded-c++"
code_path="$root_path/imbridge/code.py"

raven_test_path="$root_path/build/imbridge_test/raven"
raven_code_template_path="$root_path/imbridge_test/raven"

tpcx_ai_test_path="$root_path/build/imbridge_test/tpcx_ai"
tpcx_ai_code_template_path="$root_path/imbridge_test/tpcx_ai"

if [ "$1" == "pf" ]
then
    rm -rf /dev/shm/*
    cat "$raven_code_template_path/$1$2_code_template.py" > $code_path
    $raven_test_path/$1$2
elif [ "$1" == "uc" ]
then
    rm -rf /dev/shm/*
    cat "$tpcx_ai_code_template_path/$1$2_code_template.py" > $code_path
    $tpcx_ai_test_path/$1$2
else
    echo "no support dataset $1"
fi

sleep 4