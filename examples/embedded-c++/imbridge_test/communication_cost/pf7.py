import pickle

import duckdb
import numpy as np
from duckdb.typing import BIGINT, DOUBLE, FLOAT, VARCHAR, BOOLEAN
import pandas as pd
import time
import time
from tqdm import tqdm
import sys
# hand_type = "udf"
hand_type = "special"
name = "pf1"


con = duckdb.connect(
    "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/db/db_raven.db")

name = "pf7_used_data"

def udf(V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11, V12, V13, V14, V15, V16, V17, V18, V19, V20, V21, V22, V23, V24, V25, V26, V27, V28, Amount)->pd.Series:
    return V1


con.create_function("udf", udf,
                    [DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE,
                     DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE,
                     DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE], BIGINT, type="arrow", null_handling=hand_type)

con.sql("SET threads TO 1;")

sql1 = f'''
explain analyze SELECT udf(V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11, V12, V13, V14, V15,
 V16, V17, V18, V19, V20, V21, V22, V23, V24, V25, V26, V27, V28, Amount)
FROM {name};
'''

sql2 = f"""
explain analyze SELECT V1
FROM {name};
"""

if sys.argv[1] == "udf":
    s = time.perf_counter()
    con.sql(sql1)
    e = time.perf_counter()
    t = e-s
    print(f"udf : {t}")
else:
    s = time.perf_counter()
    con.sql(sql2)
    e = time.perf_counter()
    t = e-s
    print(f"origin : {t}")


