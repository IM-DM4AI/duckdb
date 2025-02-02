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

con = duckdb.connect(
    "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/db/db_tpcx_ai_sf10.db")

name = "uc10_used_data"

def udf(business_hour_norm, amount_norm):
    return business_hour_norm


con.create_function("udf", udf, [DOUBLE, DOUBLE], DOUBLE, type="arrow", null_handling=hand_type)

con.sql("SET threads TO 1;")

sql1 = f'''
explain analyze select udf(amount_norm, business_hour_norm) 
FROM {name};
'''

sql2 = f"""
explain analyze select amount_norm
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


